// BowlingFix - entry point.
// Logos "internal" generator = plain ObjC method swizzling, so no Substrate/ElleKit is
// needed and it works when injected into a sideloaded (non-jailbroken) app.
%config(generator=internal);

#import <UIKit/UIKit.h>
#import "BFShared.h"

// Shake to open the menu (UIKit route). Shake.mm has a CoreMotion backup because
// Unity games don't always pass shake events up to the window.
%hook UIWindow
- (void)motionEnded:(UIEventSubtype)motion withEvent:(UIEvent *)event {
    %orig;
    if (motion == UIEventSubtypeMotionShake) BFHandleShake(@"uikit");
}
%end

%ctor {
    BFStart();
}
