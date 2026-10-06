// An optional on-screen alternative to shaking the phone: a small round button, shown near the top of the
// screen by default, that opens the menu on a tap. Press and drag it anywhere on screen; its spot is
// remembered (fraction of the screen, so it survives rotation and different devices). Shake keeps working
// regardless - this is an extra way in, not a replacement, since it's one more thing that could end up
// dragged into an awkward spot or otherwise missed.
#import <UIKit/UIKit.h>
#import "BFShared.h"

// Defined further down; the button's touch handling below uses them, and Objective-C++ (like C++) needs a
// function declared before its first use.
static CGPoint BFMenuButtonClampedCenter(CGPoint p, UIView *host, CGSize size);
static void BFMenuButtonSavePosition(CGPoint center, CGSize hostSize);

@interface BFMenuButtonView : UIView
@property (nonatomic) CGPoint dragStart;     // the button's center when a touch began
@property (nonatomic) CGPoint touchStart;    // that touch's location, in the window
@property (nonatomic) BOOL dragging;
@end

@implementation BFMenuButtonView
- (instancetype)init {
    if (self = [super initWithFrame:CGRectMake(0, 0, 48, 48)]) {
        self.backgroundColor = [UIColor colorWithWhite:0.12 alpha:0.82];
        self.layer.cornerRadius = 24;
        self.layer.shadowColor = UIColor.blackColor.CGColor;
        self.layer.shadowOpacity = 0.4;
        self.layer.shadowRadius = 6;
        self.layer.shadowOffset = CGSizeMake(0, 2);
        UILabel *l = [[UILabel alloc] initWithFrame:self.bounds];
        l.text = @"\U0001F3B3";
        l.font = [UIFont systemFontOfSize:22];
        l.textAlignment = NSTextAlignmentCenter;
        l.userInteractionEnabled = NO;
        [self addSubview:l];
    }
    return self;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *t = touches.anyObject;
    self.touchStart = [t locationInView:self.superview];
    self.dragStart = self.center;
    self.dragging = NO;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *t = touches.anyObject;
    CGPoint p = [t locationInView:self.superview];
    CGFloat dx = p.x - self.touchStart.x, dy = p.y - self.touchStart.y;
    if (!self.dragging && hypot(dx, dy) < 10) return;   // still a tap, not yet a drag
    self.dragging = YES;
    self.center = BFMenuButtonClampedCenter(CGPointMake(self.dragStart.x + dx, self.dragStart.y + dy), self.superview, self.bounds.size);
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (self.dragging) BFMenuButtonSavePosition(self.center, self.superview.bounds.size);
    else BFMenuToggle();
    self.dragging = NO;
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { self.dragging = NO; }
@end

// posX/posY: fraction of the screen (0..1); -1 = not loaded yet this run.
static CGFloat sPosX = -1, sPosY = -1;
static BFMenuButtonView *sButton;

static NSString *const kDefX = @"BPMenuButtonX", *const kDefY = @"BPMenuButtonY";

static CGPoint BFMenuButtonClampedCenter(CGPoint p, UIView *host, CGSize size) {
    if (!host) return p;
    CGFloat margin = size.width / 2, top = host.safeAreaInsets.top + margin, bottom = host.bounds.size.height - host.safeAreaInsets.bottom - margin;
    CGFloat left = margin, right = host.bounds.size.width - margin;
    return CGPointMake(MIN(MAX(p.x, left), MAX(left, right)), MIN(MAX(p.y, top), MAX(top, bottom)));
}

static void BFMenuButtonSavePosition(CGPoint center, CGSize hostSize) {
    if (hostSize.width <= 0 || hostSize.height <= 0) return;
    sPosX = center.x / hostSize.width;
    sPosY = center.y / hostSize.height;
    [[NSUserDefaults standardUserDefaults] setDouble:sPosX forKey:kDefX];
    [[NSUserDefaults standardUserDefaults] setDouble:sPosY forKey:kDefY];
}

static void BFMenuButtonLoadPosition(void) {
    if (sPosX >= 0) return;
    NSUserDefaults *d = [NSUserDefaults standardUserDefaults];
    sPosX = [d objectForKey:kDefX] ? [d doubleForKey:kDefX] : 0.5;
    sPosY = [d objectForKey:kDefY] ? [d doubleForKey:kDefY] : 0.10;   // top-center by default
}

void BFMenuButtonStart(void) {
    BFMenuButtonRefresh();
    // Startup can run before the game's window is ready (or while the privacy page's window is in front), so
    // look again each time the app comes to the front. Refresh is cheap and does nothing if all is in place.
    static id sObserver;
    if (!sObserver)
        sObserver = [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification object:nil
                                                                      queue:[NSOperationQueue mainQueue]
                                                                 usingBlock:^(NSNotification *note) { BFMenuButtonRefresh(); }];
}

// The game's own window (the app delegate's), the same one Privacy.mm treats as "the game". Not simply the key
// window: at startup the key window can be the privacy SDK's own window, which BowlingPlus makes invisible
// (alpha 0) and which goes away afterwards - a button put there would be invisible, then gone.
static UIWindow *BFMenuButtonHost(void) {
    id d = [UIApplication sharedApplication].delegate;
    UIWindow *main = [d respondsToSelector:@selector(window)] ? [d window] : nil;
    if (main && !main.hidden) return main;
    for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) continue;
        for (UIWindow *w in ((UIWindowScene *)scene).windows) if (w.isKeyWindow) return w;
    }
    return nil;
}

// Call whenever it might need to appear/disappear/move: after the setting changes, when the menu opens or
// closes, on rotation (BFMenuButtonRefresh is cheap, so callers don't need to be careful about calling it
// often).
void BFMenuButtonRefresh(void) {
    UIWindow *host = BFMenuButtonHost();
    BOOL want = gBF.menuButton && host != nil && !BFMenuVisible() && !BFMenuPickerVisible();
    if (!want) { [sButton removeFromSuperview]; return; }
    if (sButton && sButton.superview != host) [sButton removeFromSuperview];
    if (!sButton) { BFMenuButtonLoadPosition(); sButton = [BFMenuButtonView new]; }
    if (!sButton.superview) {   // only when (re)added: a panel opened on top of it (the oil library) stays on top
        [host addSubview:sButton];
        sButton.center = BFMenuButtonClampedCenter(CGPointMake(sPosX * host.bounds.size.width, sPosY * host.bounds.size.height), host, sButton.bounds.size);
        [host bringSubviewToFront:sButton];
    }
}
