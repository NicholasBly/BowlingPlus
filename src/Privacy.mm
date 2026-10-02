#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import "BFShared.h"

// The every-launch privacy page
// -----------------------------
// At startup the game always asks My.Games' privacy SDK (MRGS) to show its agreement page
// (MRGSIntegration.ShowUserAgreement -> showAgreementFromFile, even after you agreed). The SDK
// is supposed to skip it for people who already agreed, but in a sideloaded copy it shows the
// "Terms and Privacy Policies ... Sign up" page (Data/Raw/MRGSGdpr/WPSmrgsgdpr.html) every time.
//
// With the option on, we press that page's own Sign up button for you: the button just calls the
// page's clickButton() function (which submits the page's form to the SDK), so we call that same
// function. Only the first-time page that says "By clicking Sign up" is pressed. The "updated
// documents" page (By clicking I agree) is always left for you to read. If anything doesn't match,
// nothing happens and you tap it yourself like before.

static int sAccepted = 0;
static NSTimer *sTimer;
static CFAbsoluteTime sStart;
static const void *kCheckedKey = &kCheckedKey;

static NSString *const kAcceptJS =
    @"(function(){var t=document.body?document.body.innerText:'';"
     "if(!t)return 'empty';"
     "if(t.indexOf('By clicking Sign up')<0)return 'no';"
     "if(typeof clickButton!=='function')return 'nofn';"
     "clickButton();return 'ok';})()";

int BFPrivacyAcceptCount(void) { return sAccepted; }

static void CollectWebViews(UIView *v, Class wk, NSMutableArray *out) {
    if ([v isKindOfClass:wk]) [out addObject:v];
    for (UIView *sub in v.subviews) CollectWebViews(sub, wk, out);
}

static NSArray<UIWindow *> *AllWindows(void) {
    NSMutableArray<UIWindow *> *wins = [NSMutableArray array];
    for (UIScene *scene in [UIApplication sharedApplication].connectedScenes)
        if ([scene isKindOfClass:[UIWindowScene class]]) [wins addObjectsFromArray:((UIWindowScene *)scene).windows];
    return wins;
}

bool BFPrivacyPageVisible(void) {      // any web page on screen (the SDK shows its pages in a WKWebView)
    Class wk = NSClassFromString(@"WKWebView");
    if (!wk) return false;
    NSMutableArray<UIView *> *webViews = [NSMutableArray array];
    for (UIWindow *w in AllWindows()) if (!w.hidden) CollectWebViews(w, wk, webViews);
    for (UIView *v in webViews) if (v.window && !v.hidden && v.alpha > 0.01) return true;
    return false;
}

static void PrivacyTick(void) {
    if (CFAbsoluteTimeGetCurrent() - sStart > 180) {   // the page only shows while the game starts
        [sTimer invalidate];
        sTimer = nil;
        return;
    }
    if (!gBF.autoPrivacy) return;
    Class wk = NSClassFromString(@"WKWebView");
    SEL eval = NSSelectorFromString(@"evaluateJavaScript:completionHandler:");
    if (!wk) return;
    NSMutableArray<UIView *> *webViews = [NSMutableArray array];
    for (UIWindow *w in AllWindows()) CollectWebViews(w, wk, webViews);
    for (UIView *web in webViews) {
        if (![web respondsToSelector:eval] || [[web valueForKey:@"loading"] boolValue]) continue;
        NSString *page = [[web valueForKey:@"URL"] absoluteString] ?: @"";
        if ([objc_getAssociatedObject(web, kCheckedKey) isEqualToString:page]) continue;   // already looked at this page
        objc_setAssociatedObject(web, kCheckedKey, page, OBJC_ASSOCIATION_COPY_NONATOMIC);
        __weak UIView *weakWeb = web;
        void (^done)(id, NSError *) = ^(id result, NSError *error) {
            if ([result isEqual:@"ok"]) {
                sAccepted++;
                BFLog(@"privacy page: pressed Sign up for you");
            } else if ([result isEqual:@"empty"] || [result isEqual:@"nofn"]) {
                objc_setAssociatedObject(weakWeb, kCheckedKey, nil, OBJC_ASSOCIATION_COPY_NONATOMIC);   // not ready yet: look again
            }
        };
        ((void (*)(id, SEL, NSString *, id))objc_msgSend)(web, eval, kAcceptJS, done);
    }
}

void BFPrivacyStart(void) {
    if (sTimer) return;
    sStart = CFAbsoluteTimeGetCurrent();
    sTimer = [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:YES block:^(NSTimer *t) { PrivacyTick(); }];
}
