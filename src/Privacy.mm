#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import "BFShared.h"

// The every-launch privacy page
// -----------------------------
// At startup the game always asks My.Games' privacy SDK (MRGS) to show its agreement page
// (MRGSIntegration.ShowUserAgreement -> showAgreementFromFile, even after you agreed). The SDK is supposed
// to skip it for people who already agreed, but in a sideloaded copy it shows the "Terms and Privacy
// Policies ... Sign up" page (Data/Raw/MRGSGdpr/WPSmrgsgdpr.html) every time.
//
// BowlingPlus handles it in two steps, with no setting to manage:
//  1. The first time, nothing is touched. You accept the page yourself. When that page goes away,
//     BowlingPlus remembers it (gBF.privacyOK).
//  2. From then on, as soon as the page's web view shows up it is hidden (alpha 0, so you never see it) and
//     the page's own Sign up button is pressed for you: the button just calls the page's clickButton()
//     function, which submits the page's form to the SDK, so we call that same function.
// Only the first-time page that says "By clicking Sign up" is pressed. Any other page (for example the
// "updated documents" page, "By clicking I agree") is brought back into view and left for you to read.
// If a page never finishes loading, it is shown again after 8 s. If anything doesn't match, nothing is
// hidden for long and you tap it yourself like before.

static int sAccepted = 0, sHiddenCount = 0;
static NSTimer *sTimer;
static CFAbsoluteTime sStart;
static BOOL sWatching = NO;                       // first time: a Sign-up page is on screen, waiting for you to accept it
static __weak UIView *sWatched;
static const void *kStateKey = &kStateKey, *kSinceKey = &kSinceKey, *kProbeKey = &kProbeKey,
                  *kHiddenKey = &kHiddenKey, *kWinKey = &kWinKey;
enum { ST_NEW = 0, ST_WAIT, ST_SIGNUP, ST_OTHER, ST_PRESSED };

static NSString *const kProbeJS =
    @"(function(){var t=document.body?document.body.innerText:'';"
     "if(!t)return 'empty';"
     "if(t.indexOf('By clicking Sign up')<0)return 'other';"
     "if(typeof clickButton!=='function')return 'nofn';"
     "return 'signup';})()";
static NSString *const kAcceptJS = @"(function(){if(typeof clickButton!=='function')return 'nofn';clickButton();return 'ok';})()";

int BFPrivacyAcceptCount(void) { return sAccepted; }
NSString *BFPrivacyDebug(void) {
    return [NSString stringWithFormat:@"privacy: remembered=%d pressedForYou=%d hiddenPages=%d watching=%d",
            gBF.privacyOK, sAccepted, sHiddenCount, sWatching];
}

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

// ---- keeping the page out of sight (never the game itself) ----
static UIWindow *MainWindow(void) {
    id d = [UIApplication sharedApplication].delegate;
    return [d respondsToSelector:@selector(window)] ? [d window] : nil;
}

static void Conceal(UIView *web) {
    UIWindow *win = web.window;
    if (!win) return;
    UIWindow *main = MainWindow();
    BOOL ownWindow = (main && win != main) || win.windowLevel > UIWindowLevelNormal;
    if (ownWindow) {                               // the SDK's own window: hide all of it
        win.alpha = 0;
        objc_setAssociatedObject(web, kWinKey, win, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    UIView *root = win.rootViewController.view;
    UIView *v = web;                               // the top-most piece that holds only the page (not the game's own view)
    while (v.superview && v.superview != win && v.superview != root) v = v.superview;
    if (v != root) {
        v.alpha = 0;
        objc_setAssociatedObject(web, kHiddenKey, v, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
}

static void Reveal(UIView *web) {
    UIView *v = objc_getAssociatedObject(web, kHiddenKey);
    UIWindow *w = objc_getAssociatedObject(web, kWinKey);
    if (v) v.alpha = 1;
    if (w) w.alpha = 1;
    objc_setAssociatedObject(web, kHiddenKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(web, kWinKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

static void SetState(UIView *web, int st) { objc_setAssociatedObject(web, kStateKey, @(st), OBJC_ASSOCIATION_RETAIN_NONATOMIC); }

static void Eval(UIView *web, NSString *js, void (^done)(id)) {
    SEL eval = NSSelectorFromString(@"evaluateJavaScript:completionHandler:");
    if (![web respondsToSelector:eval]) return;
    void (^cb)(id, NSError *) = ^(id result, NSError *error) { done(result); };
    ((void (*)(id, SEL, NSString *, id))objc_msgSend)(web, eval, js, cb);
}

static void PrivacyTick(void) {
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent(), age = now - sStart;
    // you accepted the page yourself: it went away while we were watching it
    if (sWatching && (!sWatched || !sWatched.window || sWatched.hidden)) {
        sWatching = NO;
        if (!gBF.privacyOK) {
            gBF.privacyOK = true;
            BFSaveConfig();
            BFLog(@"privacy page: you accepted it - BowlingPlus remembers that and will press it for you, out of sight, from now on");
        }
    }
    if (age > 300 && !sWatching) {                 // the page only shows while the game starts
        [sTimer invalidate];
        sTimer = nil;
        return;
    }
    if (gBFSafeMode) return;
    Class wk = NSClassFromString(@"WKWebView");
    if (!wk) return;
    NSMutableArray<UIView *> *webViews = [NSMutableArray array];
    for (UIWindow *w in AllWindows()) CollectWebViews(w, wk, webViews);
    for (UIView *web in webViews) {
        if (!web.window) continue;
        int st = [objc_getAssociatedObject(web, kStateKey) intValue];
        double since = [objc_getAssociatedObject(web, kSinceKey) doubleValue];
        if (st == ST_NEW) {
            since = now;
            objc_setAssociatedObject(web, kSinceKey, @(since), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            st = ST_WAIT;
            SetState(web, st);
        }
        BOOL hide = gBF.privacyOK && age <= 300 && st != ST_OTHER;
        if (hide) {
            Conceal(web);                          // every frame: the SDK may fade it in
            if (st == ST_WAIT && now - since > 8) { Reveal(web); SetState(web, ST_OTHER); BFLog(@"web page didn't load in 8 s: showing it again"); continue; }
        }
        if ([[web valueForKey:@"loading"] boolValue]) continue;
        if (st != ST_WAIT && !(st == ST_SIGNUP && gBF.privacyOK)) continue;
        double lastProbe = [objc_getAssociatedObject(web, kProbeKey) doubleValue];
        if (now - lastProbe < 0.25) continue;      // don't ask the page every frame
        objc_setAssociatedObject(web, kProbeKey, @(now), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        __weak UIView *weakWeb = web;
        Eval(web, kProbeJS, ^(id result) {
            UIView *w = weakWeb;
            if (!w) return;
            if ([result isEqual:@"signup"]) {
                if (gBF.privacyOK) {               // remembered: press it for you
                    SetState(w, ST_PRESSED);
                    Eval(w, kAcceptJS, ^(id r) {
                        if ([r isEqual:@"ok"]) { sAccepted++; BFLog(@"privacy page: pressed Sign up for you (out of sight)"); }
                    });
                    sHiddenCount++;
                } else {                           // first time: you press it; we just watch for it to go away
                    SetState(w, ST_SIGNUP);
                    sWatching = YES;
                    sWatched = w;
                }
            } else if ([result isEqual:@"other"]) {
                SetState(w, ST_OTHER);
                Reveal(w);                         // some other page (updated terms, news...): yours to read
            }                                      // 'empty' / 'nofn': not ready yet, ask again
        });
    }
}

void BFPrivacyStart(void) {
    if (sTimer) return;
    sStart = CFAbsoluteTimeGetCurrent();
    sTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 30.0) repeats:YES block:^(NSTimer *t) { PrivacyTick(); }];
    [[NSRunLoop mainRunLoop] addTimer:sTimer forMode:NSRunLoopCommonModes];
}
