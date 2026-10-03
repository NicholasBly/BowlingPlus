#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import <PhotosUI/PhotosUI.h>
#import <CoreImage/CoreImage.h>
#import <objc/runtime.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <PDFKit/PDFKit.h>
#import "BFShared.h"

// Custom oil patterns
// -------------------
// A pattern is what you'd program into a real Kegel lane machine: forward and reverse load steps
// (start board, stop board, loads, speed). The game's own Kegel engine turns it into oil (Game.mm,
// "Oil"), so custom patterns behave exactly like the built-in ones. Saved in NSUserDefaults and
// shared as a QR code or text code: "BJBOIL1:" + base64url(zlib(JSON)).

static NSString *const kPatternsKey = @"BowlingPlus.oilPatterns";
static NSString *const kActiveKey = @"BowlingPlus.oilActive";
static NSString *const kCodePrefix = @"BJBOIL1:";

#pragma mark - theme (the game's look: dark panels, yellow tabs, orange buttons, rounded type)

static UIColor *OAccent(void) { return [UIColor colorWithRed:1.0 green:0.48 blue:0.10 alpha:1]; }
static UIColor *OYellow(void) { return [UIColor colorWithRed:1.0 green:0.80 blue:0.24 alpha:1]; }
static UIColor *OPanel(void) { return [UIColor colorWithRed:0.08 green:0.09 blue:0.11 alpha:0.97]; }
static UIColor *ODim(CGFloat a) { return [UIColor colorWithWhite:1 alpha:a]; }
static UIFont *OFont(CGFloat size, UIFontWeight w) {
    UIFont *f = [UIFont systemFontOfSize:size weight:w];
    UIFontDescriptor *d = [f.fontDescriptor fontDescriptorWithDesign:UIFontDescriptorSystemDesignRounded];
    return d ? [UIFont fontWithDescriptor:d size:size] : f;
}

static UIWindow *OHostWindow(void) {
    UIWindow *fallback = nil;
    for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) continue;
        for (UIWindow *w in ((UIWindowScene *)scene).windows) {
            if (w.windowLevel > UIWindowLevelAlert) continue;      // our presenter window
            if (w.isKeyWindow) return w;
            if (!fallback && !w.hidden) fallback = w;
        }
    }
    return fallback;
}

static UIViewController *OTopController(void) {
    UIViewController *vc = OHostWindow().rootViewController;
    while (vc.presentedViewController) vc = vc.presentedViewController;
    return vc;
}

static UILabel *OLabel(NSString *text, CGFloat size, UIFontWeight w, UIColor *color) {
    UILabel *l = [UILabel new];
    l.text = text;
    l.font = OFont(size, w);
    l.textColor = color;
    l.numberOfLines = 0;
    return l;
}

static UIButton *OButton(NSString *title, BOOL filled, id target, SEL action) {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    [b setTitle:title forState:UIControlStateNormal];
    [b setTitleColor:filled ? [UIColor colorWithWhite:0.08 alpha:1] : UIColor.whiteColor forState:UIControlStateNormal];
    b.titleLabel.font = OFont(15, UIFontWeightBold);
    b.titleLabel.adjustsFontSizeToFitWidth = YES;
    b.backgroundColor = filled ? OYellow() : ODim(0.12);
    b.layer.cornerRadius = 12;
    b.contentEdgeInsets = UIEdgeInsetsMake(11, 14, 11, 14);
    if (target && action) [b addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];
    return b;
}

static UIStackView *OStack(NSArray<UIView *> *views, UILayoutConstraintAxis axis, CGFloat spacing) {
    UIStackView *s = [[UIStackView alloc] initWithArrangedSubviews:views ?: @[]];
    s.axis = axis;
    s.spacing = spacing;
    if (axis == UILayoutConstraintAxisHorizontal) s.alignment = UIStackViewAlignmentCenter;
    return s;
}

static UIView *OCard(void) {
    UIView *c = [UIView new];
    c.translatesAutoresizingMaskIntoConstraints = NO;
    c.backgroundColor = OPanel();
    c.layer.cornerRadius = 22;
    c.layer.borderWidth = 1;
    c.layer.borderColor = ODim(0.08).CGColor;
    c.clipsToBounds = YES;
    return c;
}

// a full-window dimmed layer with a scrolling card; returns the card's content stack
static UIStackView *OSheet(UIView **overlayOut, UIView **cardOut, BOOL top) {
    UIWindow *w = OHostWindow();
    UIView *overlay = [[UIView alloc] initWithFrame:w.bounds];
    overlay.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    overlay.backgroundColor = [UIColor colorWithWhite:0 alpha:0.5];
    UIView *card = OCard();
    [overlay addSubview:card];
    UIScrollView *scroll = [UIScrollView new];
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    scroll.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
    scroll.alwaysBounceVertical = YES;
    [card addSubview:scroll];
    UIStackView *stack = OStack(nil, UILayoutConstraintAxisVertical, 12);
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    stack.layoutMargins = UIEdgeInsetsMake(16, 16, 18, 16);
    stack.layoutMarginsRelativeArrangement = YES;
    [scroll addSubview:stack];
    CGFloat width = MAX(280, MIN(430, w.bounds.size.width - 16));
    NSLayoutConstraint *fit = [scroll.heightAnchor constraintEqualToAnchor:stack.heightAnchor];
    fit.priority = UILayoutPriorityDefaultLow;
    [NSLayoutConstraint activateConstraints:@[
        [card.centerXAnchor constraintEqualToAnchor:overlay.centerXAnchor],
        [card.widthAnchor constraintEqualToConstant:width],
        top ? [card.topAnchor constraintEqualToAnchor:overlay.safeAreaLayoutGuide.topAnchor constant:6]
            : [card.centerYAnchor constraintEqualToAnchor:overlay.centerYAnchor],
        [card.heightAnchor constraintLessThanOrEqualToAnchor:overlay.safeAreaLayoutGuide.heightAnchor constant:-12],
        [scroll.topAnchor constraintEqualToAnchor:card.topAnchor],
        [scroll.bottomAnchor constraintEqualToAnchor:card.bottomAnchor],
        [scroll.leadingAnchor constraintEqualToAnchor:card.leadingAnchor],
        [scroll.trailingAnchor constraintEqualToAnchor:card.trailingAnchor],
        [stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor],
        [stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor],
        [stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor],
        fit,
    ]];
    if (overlayOut) *overlayOut = overlay;
    if (cardOut) *cardOut = card;
    return stack;
}

static void OPresent(UIView *overlay, UIView *card, BOOL fromTop) {
    UIWindow *w = OHostWindow();
    if (!w) return;
    overlay.frame = w.bounds;
    [w addSubview:overlay];
    [overlay layoutIfNeeded];
    overlay.alpha = 0;
    card.transform = CGAffineTransformMakeTranslation(0, fromTop ? -40 : 30);
    [UIView animateWithDuration:0.22 delay:0 usingSpringWithDamping:0.9 initialSpringVelocity:0 options:0 animations:^{
        overlay.alpha = 1;
        card.transform = CGAffineTransformIdentity;
    } completion:nil];
}

static void ODismiss(UIView *overlay) {
    [UIView animateWithDuration:0.16 animations:^{ overlay.alpha = 0; } completion:^(BOOL f) { [overlay removeFromSuperview]; }];
}

static void OToast(NSString *text) {
    UIWindow *w = OHostWindow();
    if (!w) return;
    UILabel *l = OLabel(text, 14, UIFontWeightSemibold, UIColor.whiteColor);
    l.textAlignment = NSTextAlignmentCenter;
    l.backgroundColor = [UIColor colorWithWhite:0.05 alpha:0.92];
    l.layer.cornerRadius = 12;
    l.clipsToBounds = YES;
    l.translatesAutoresizingMaskIntoConstraints = NO;
    [w addSubview:l];
    [NSLayoutConstraint activateConstraints:@[
        [l.centerXAnchor constraintEqualToAnchor:w.centerXAnchor],
        [l.bottomAnchor constraintEqualToAnchor:w.safeAreaLayoutGuide.bottomAnchor constant:-24],
        [l.widthAnchor constraintLessThanOrEqualToAnchor:w.widthAnchor constant:-40],
        [l.heightAnchor constraintGreaterThanOrEqualToConstant:40],
    ]];
    l.text = [NSString stringWithFormat:@"   %@   ", text];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.2 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [UIView animateWithDuration:0.2 animations:^{ l.alpha = 0; } completion:^(BOOL f) { [l removeFromSuperview]; }];
    });
}

#pragma mark - our own choice panel (iOS pop-up menus don't open inside the game)

// items: [{title, action (block), primary?, destructive?, confirm? (second tap needed)}]
static void OChoose(NSString *title, NSString *subtitle, NSArray<NSDictionary *> *items) {
    UIView *overlay, *card;
    UIStackView *stack = OSheet(&overlay, &card, NO);
    if (!overlay) return;
    [stack addArrangedSubview:OLabel(title, 19, UIFontWeightHeavy, UIColor.whiteColor)];
    if (subtitle.length) [stack addArrangedSubview:OLabel(subtitle, 12, UIFontWeightRegular, ODim(0.55))];
    __weak UIView *weakOverlay = overlay;
    for (NSDictionary *it in items) {
        UIButton *b = OButton(it[@"title"], [it[@"primary"] boolValue], nil, nil);
        b.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
        if ([it[@"destructive"] boolValue]) [b setTitleColor:[UIColor colorWithRed:1 green:0.4 blue:0.35 alpha:1] forState:UIControlStateNormal];
        void (^action)(void) = it[@"action"];
        BOOL confirm = [it[@"confirm"] boolValue];
        __block BOOL armed = NO;
        __weak UIButton *wb = b;
        [b addAction:[UIAction actionWithHandler:^(UIAction *a) {
            if (confirm && !armed) {               // a second tap confirms (no system alert needed)
                armed = YES;
                [wb setTitle:@"Tap again to confirm" forState:UIControlStateNormal];
                return;
            }
            ODismiss(weakOverlay);
            if (action) action();
        }] forControlEvents:UIControlEventTouchUpInside];
        [stack addArrangedSubview:b];
    }
    UIButton *cancel = OButton(@"Cancel", NO, nil, nil);
    [cancel addAction:[UIAction actionWithHandler:^(UIAction *a) { ODismiss(weakOverlay); }] forControlEvents:UIControlEventTouchUpInside];
    [stack addArrangedSubview:cancel];
    OPresent(overlay, card, NO);
}

// Show a system screen (share sheet, photo picker) from a small window of our own, so it doesn't
// depend on the game's own view controllers.
static UIWindow *sPresenterWindow;
static void OPresentVC(UIViewController *vc) {
    UIWindow *game = OHostWindow();
    if (!sPresenterWindow && game.windowScene) {
        sPresenterWindow = [[UIWindow alloc] initWithWindowScene:game.windowScene];
        sPresenterWindow.windowLevel = UIWindowLevelAlert + 1;
        sPresenterWindow.backgroundColor = UIColor.clearColor;
        sPresenterWindow.rootViewController = [UIViewController new];
        sPresenterWindow.rootViewController.view.backgroundColor = UIColor.clearColor;
    }
    UIViewController *root = sPresenterWindow.rootViewController;
    if (!root) { [OTopController() presentViewController:vc animated:YES completion:nil]; return; }
    sPresenterWindow.hidden = NO;
    [root presentViewController:vc animated:YES completion:nil];
}
static void ODonePresenting(void) {             // hide our window and give the game its window back
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.4 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (sPresenterWindow.rootViewController.presentedViewController) return;
        sPresenterWindow.hidden = YES;
        for (UIScene *scene in [UIApplication sharedApplication].connectedScenes)
            if ([scene isKindOfClass:[UIWindowScene class]])
                for (UIWindow *w in ((UIWindowScene *)scene).windows)
                    if (w != sPresenterWindow && !w.hidden) { [w makeKeyWindow]; return; }
    });
}

#pragma mark - storage

static NSMutableArray<NSDictionary *> *LoadPatterns(void) {
    NSMutableArray *out = [NSMutableArray array];
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    for (id p in [ud arrayForKey:kPatternsKey] ?: [ud arrayForKey:@"BowlingFix.oilPatterns"])   // or saved before the rename
        if ([p isKindOfClass:[NSDictionary class]] && [p[@"id"] isKindOfClass:[NSString class]]) [out addObject:p];
    return out;
}
static void SavePatterns(NSArray *patterns) { [[NSUserDefaults standardUserDefaults] setObject:patterns forKey:kPatternsKey]; }
static NSString *ActiveId(void) {
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    if ([ud objectForKey:kActiveKey]) return [ud stringForKey:kActiveKey];
    return [ud stringForKey:@"BowlingFix.oilActive"];                 // saved before the rename
}
static NSDictionary *PatternById(NSString *pid) {
    if (!pid.length) return nil;
    for (NSDictionary *p in BFOilCollection()) if ([p[@"id"] isEqualToString:pid]) return p;
    for (NSDictionary *p in LoadPatterns()) if ([p[@"id"] isEqualToString:pid]) return p;
    return nil;
}
static void ApplyActive(void) { BFOilSetCustom(PatternById(ActiveId())); }
static void SetActiveId(NSString *pid) {
    [[NSUserDefaults standardUserDefaults] setObject:pid ?: @"" forKey:kActiveKey];   // "" = off (so the old name isn't used)
    ApplyActive();
}
static void UpsertPattern(NSDictionary *p) {
    NSMutableArray *all = LoadPatterns();
    NSUInteger i = [all indexOfObjectPassingTest:^BOOL(NSDictionary *x, NSUInteger idx, BOOL *stop) { return [x[@"id"] isEqualToString:p[@"id"]]; }];
    if (i == NSNotFound) [all addObject:p]; else all[i] = p;
    SavePatterns(all);
    if ([p[@"id"] isEqualToString:ActiveId()]) ApplyActive();
}

#pragma mark - steps, sharing codes

static int Clampi(id v, int lo, int hi, int def) {
    if (![v respondsToSelector:@selector(intValue)]) return def;
    int x = [v intValue];
    return x < lo ? lo : x > hi ? hi : x;
}

// [[start, stop, loads, speed, travelToFeet], ...]. Boards are 1-39 from the left (38 = 2R), like the
// game's Kegel files. The engine works out each step's end from loads x speed; travelToFeet only
// matters for zero-load (travel-only) steps, exactly like a real Kegel machine.
static NSArray *CleanSteps(id raw) {
    NSMutableArray *out = [NSMutableArray array];
    if (![raw isKindOfClass:[NSArray class]]) return out;
    for (id s in raw) {
        if (![s isKindOfClass:[NSArray class]] || [s count] < 4 || out.count >= 40) continue;
        int a = Clampi(s[0], 1, 39, 2), b = Clampi(s[1], 1, 39, 38);
        float ft = [s count] > 4 && [s[4] respondsToSelector:@selector(floatValue)] ? [s[4] floatValue] : 0;
        ft = roundf(fminf(fmaxf(ft, 0), 70) * 100) / 100;     // keep Kegel files' exact distances (3.92 ft)
        [out addObject:@[ @(MIN(a, b)), @(MAX(a, b)), @(Clampi(s[2], 0, 99, 2)), @(Clampi(s[3], 6, 30, 14)), @(ft) ]];
    }
    return out;
}

static NSString *BoardLabel(int b) {          // Kegel boards 1-39: 2 = "2L", 38 = "2R"
    if (b < 20) return [NSString stringWithFormat:@"%dL", b];
    if (b == 20) return @"20";
    return [NSString stringWithFormat:@"%dR", 40 - b];
}

static NSString *EncodePattern(NSDictionary *p) {
    NSDictionary *j = @{ @"v": @1, @"n": p[@"name"] ?: @"Custom", @"b": p[@"base"] ?: @0,
                         @"f": CleanSteps(p[@"fwd"]), @"r": CleanSteps(p[@"rev"]), @"d": @(Clampi(p[@"drop"], 0, 60, 0)), @"x": @([p[@"exact"] boolValue]) };
    NSData *json = [NSJSONSerialization dataWithJSONObject:j options:0 error:nil];
    NSData *z = [json compressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib error:nil] ?: json;
    NSString *b = [z base64EncodedStringWithOptions:0];
    b = [[[b stringByReplacingOccurrencesOfString:@"+" withString:@"-"] stringByReplacingOccurrencesOfString:@"/" withString:@"_"]
         stringByReplacingOccurrencesOfString:@"=" withString:@""];
    return [kCodePrefix stringByAppendingString:b];
}

static NSDictionary *DecodePattern(NSString *code) {
    NSRange r = [code rangeOfString:kCodePrefix];
    if (!code || r.location == NSNotFound) return nil;
    NSString *b = [[[code substringFromIndex:NSMaxRange(r)] componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] firstObject];
    b = [[b stringByReplacingOccurrencesOfString:@"-" withString:@"+"] stringByReplacingOccurrencesOfString:@"_" withString:@"/"];
    while (b.length % 4) b = [b stringByAppendingString:@"="];
    NSData *z = [[NSData alloc] initWithBase64EncodedString:b options:0];
    if (!z) return nil;
    NSData *json = [z decompressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib error:nil] ?: z;
    NSDictionary *j = [NSJSONSerialization JSONObjectWithData:json options:0 error:nil];
    if (![j isKindOfClass:[NSDictionary class]]) return nil;
    NSArray *f = CleanSteps(j[@"f"]), *rv = CleanSteps(j[@"r"]);
    if (!f.count && !rv.count) return nil;
    NSString *name = [j[@"n"] isKindOfClass:[NSString class]] ? [j[@"n"] substringToIndex:MIN((NSUInteger)40, [j[@"n"] length])] : @"Imported";
    return @{ @"id": NSUUID.UUID.UUIDString, @"name": name, @"base": @(Clampi(j[@"b"], 0, 999, 0)), @"fwd": f, @"rev": rv,
              @"drop": @(Clampi(j[@"d"], 0, 60, 0)), @"exact": @([j[@"x"] boolValue]) };
}

#pragma mark - oil preview (thickness colors, like a Kegel sheet)

// The game's own oil colors barely change once oil is thicker than a few units, so every pattern looks
// flat. Previews use a thickness ramp instead: light cyan = thin, through blue, to navy = thick.
// The scale is fixed (the game's oil units, 0-75), so patterns compare fairly.
static const float kThickMax = 75;
static void ThicknessRGB(float v, CGFloat out[3]) {
    static const CGFloat stops[5][4] = {
        { 0.00, 0.80, 0.97, 1.00 }, { 0.25, 0.35, 0.85, 0.95 }, { 0.50, 0.20, 0.55, 0.90 },
        { 0.75, 0.12, 0.25, 0.70 }, { 1.00, 0.07, 0.10, 0.40 } };
    CGFloat t = fmin(fmax(v / kThickMax, 0), 1);
    for (int i = 1; i < 5; i++) {
        if (t <= stops[i][0]) {
            CGFloat f = (t - stops[i - 1][0]) / (stops[i][0] - stops[i - 1][0]);
            for (int k = 0; k < 3; k++) out[k] = stops[i - 1][k + 1] + (stops[i][k + 1] - stops[i - 1][k + 1]) * f;
            return;
        }
    }
    for (int k = 0; k < 3; k++) out[k] = stops[4][k + 1];
}

static UIView *OLegend(void) {                 // [Thin ====gradient==== Thick]
    CGSize sz = CGSizeMake(160, 10);
    UIImage *bar = [[[UIGraphicsImageRenderer alloc] initWithSize:sz] imageWithActions:^(UIGraphicsImageRendererContext *ctx) {
        for (int x = 0; x < (int)sz.width; x++) {
            CGFloat c[3];
            ThicknessRGB((x + 1) / sz.width * kThickMax, c);
            [[UIColor colorWithRed:c[0] green:c[1] blue:c[2] alpha:1] setFill];
            UIRectFill(CGRectMake(x, 0, 1, sz.height));
        }
    }];
    UIImageView *iv = [[UIImageView alloc] initWithImage:bar];
    iv.layer.cornerRadius = 4;
    iv.clipsToBounds = YES;
    [iv.widthAnchor constraintEqualToConstant:sz.width].active = YES;
    [iv.heightAnchor constraintEqualToConstant:sz.height].active = YES;
    UIStackView *row = OStack(@[OLabel(@"Thin oil", 11, UIFontWeightSemibold, ODim(0.5)), iv,
                                OLabel(@"Thick oil", 11, UIFontWeightSemibold, ODim(0.5))], UILayoutConstraintAxisHorizontal, 8);
    UIView *wrap = [UIView new];
    row.translatesAutoresizingMaskIntoConstraints = NO;
    [wrap addSubview:row];
    [NSLayoutConstraint activateConstraints:@[
        [row.topAnchor constraintEqualToAnchor:wrap.topAnchor], [row.bottomAnchor constraintEqualToAnchor:wrap.bottomAnchor],
        [row.centerXAnchor constraintEqualToAnchor:wrap.centerXAnchor],
    ]];
    return wrap;
}

// Lane drawn sideways: foul line on the left, pins on the right, the bowler's left boards on top.
static UIImage *RenderPreview(NSDictionary *r, CGSize size) {
    int w = [r[@"w"] intValue], h = [r[@"h"] intValue];
    NSData *grid = r[@"grid"];
    if (w <= 0 || h <= 0 || grid.length < sizeof(float) * w * h) return nil;
    const float *g = (const float *)grid.bytes;
    int cols = MIN(h, 480), rows = w;
    NSMutableData *px = [NSMutableData dataWithLength:(size_t)cols * rows * 4];
    uint8_t *p = (uint8_t *)px.mutableBytes;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            int hy = (int)((long)x * h / cols);
            float v = g[(size_t)y * h + hy];
            CGFloat wr = 0.86, wg = 0.70, wb = 0.50, c[3];               // lane wood
            ThicknessRGB(v, c);
            CGFloat a = v > 0.01 ? 0.9 : 0;
            uint8_t *o = p + ((size_t)(rows - 1 - y) * cols + x) * 4;   // top edge = the bowler's left
            o[0] = (uint8_t)(255 * (wr * (1 - a) + c[0] * a));
            o[1] = (uint8_t)(255 * (wg * (1 - a) + c[1] * a));
            o[2] = (uint8_t)(255 * (wb * (1 - a) + c[2] * a));
            o[3] = 255;
        }
    }
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef dp = CGDataProviderCreateWithCFData((__bridge CFDataRef)px);
    CGImageRef img = CGImageCreate(cols, rows, 8, 32, cols * 4, cs, kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast, dp, NULL, false, kCGRenderingIntentDefault);
    UIGraphicsImageRenderer *ren = [[UIGraphicsImageRenderer alloc] initWithSize:size];
    UIImage *out = [ren imageWithActions:^(UIGraphicsImageRendererContext *ctx) {
        CGContextRef c = ctx.CGContext;
        CGContextSetInterpolationQuality(c, kCGInterpolationHigh);
        CGContextSaveGState(c);
        CGContextTranslateCTM(c, 0, size.height);
        CGContextScaleCTM(c, 1, -1);
        CGContextDrawImage(c, CGRectMake(0, 0, size.width, size.height), img);
        CGContextRestoreGState(c);
        [[UIColor colorWithWhite:0 alpha:0.18] setFill];               // arrows ~15 ft, foul line
        CGFloat ft = size.width / 60.0;
        UIRectFill(CGRectMake(15 * ft, 0, 1, size.height));
        UIRectFill(CGRectMake(0, 0, 2, size.height));
    }];
    CGImageRelease(img);
    CGDataProviderRelease(dp);
    CGColorSpaceRelease(cs);
    return out;
}

#pragma mark - a big, thumb-friendly stepper

@interface BFOilStepper : UIView
@property (nonatomic, copy) NSString *(^format)(int value);
@property (nonatomic, copy) void (^changed)(int value);
@property (nonatomic) int value, minValue, maxValue;
@property (nonatomic, strong) UILabel *valueLabel;
- (instancetype)initWithTitle:(NSString *)title min:(int)lo max:(int)hi;
@end

@implementation BFOilStepper
- (instancetype)initWithTitle:(NSString *)title min:(int)lo max:(int)hi {
    if ((self = [super init])) {
        _minValue = lo;
        _maxValue = hi;
        self.backgroundColor = ODim(0.07);
        self.layer.cornerRadius = 10;
        UILabel *t = OLabel(title, 10, UIFontWeightBold, ODim(0.5));
        t.textAlignment = NSTextAlignmentCenter;
        _valueLabel = OLabel(@"", 16, UIFontWeightHeavy, UIColor.whiteColor);
        _valueLabel.textAlignment = NSTextAlignmentCenter;
        UIButton *minus = [UIButton buttonWithType:UIButtonTypeSystem], *plus = [UIButton buttonWithType:UIButtonTypeSystem];
        [minus setTitle:@"\u2212" forState:UIControlStateNormal];
        [plus setTitle:@"+" forState:UIControlStateNormal];
        for (UIButton *b in @[minus, plus]) {
            b.titleLabel.font = OFont(22, UIFontWeightBold);
            [b setTitleColor:OYellow() forState:UIControlStateNormal];
            [b.widthAnchor constraintEqualToConstant:34].active = YES;
            [b.heightAnchor constraintEqualToConstant:40].active = YES;
        }
        minus.tag = -1;
        plus.tag = 1;
        [minus addTarget:self action:@selector(step:) forControlEvents:UIControlEventTouchUpInside];
        [plus addTarget:self action:@selector(step:) forControlEvents:UIControlEventTouchUpInside];
        UIStackView *mid = OStack(@[t, _valueLabel], UILayoutConstraintAxisVertical, 0);
        UIStackView *row = OStack(@[minus, mid, plus], UILayoutConstraintAxisHorizontal, 0);
        row.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:row];
        [NSLayoutConstraint activateConstraints:@[
            [row.topAnchor constraintEqualToAnchor:self.topAnchor constant:2],
            [row.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-2],
            [row.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [row.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        ]];
    }
    return self;
}
- (void)setValue:(int)value {
    _value = MAX(_minValue, MIN(_maxValue, value));
    self.valueLabel.text = self.format ? self.format(_value) : [NSString stringWithFormat:@"%d", _value];
}
- (void)step:(UIButton *)b {
    int v = MAX(self.minValue, MIN(self.maxValue, self.value + (int)b.tag));
    if (v == self.value) return;
    self.value = v;
    [[UISelectionFeedbackGenerator new] selectionChanged];
    if (self.changed) self.changed(v);
}
@end

#pragma mark - pattern editor

@interface BFOilEditor : NSObject <UITextFieldDelegate>
@property (nonatomic, strong) NSMutableDictionary *pattern;
@property (nonatomic, strong) NSMutableArray<NSMutableArray<NSNumber *> *> *fwd, *rev;
@property (nonatomic, strong) UIView *overlay, *card;
@property (nonatomic, strong) UIStackView *fwdStack, *revStack;
@property (nonatomic, strong) UITextField *nameField;
@property (nonatomic, strong) UIImageView *preview;
@property (nonatomic, strong) UILabel *stats;
@property (nonatomic, strong) UIButton *baseButton;
@property (nonatomic, strong) BFOilStepper *dropStepper;
@property (nonatomic, strong) NSMutableArray<UILabel *> *fwdEnds, *revEnds;
@property (nonatomic, copy) void (^onDone)(NSDictionary *saved);
+ (void)editPattern:(NSDictionary *)p done:(void (^)(NSDictionary *saved))done;
@end

static BFOilEditor *sEditor;

@implementation BFOilEditor

+ (void)editPattern:(NSDictionary *)p done:(void (^)(NSDictionary *))done {
    BFOilEditor *e = [BFOilEditor new];
    e.onDone = done;
    e.pattern = [p mutableCopy] ?: [NSMutableDictionary dictionary];
    if (!e.pattern[@"id"]) e.pattern[@"id"] = NSUUID.UUID.UUIDString;
    e.fwd = [NSMutableArray array];
    e.rev = [NSMutableArray array];
    for (NSArray *s in CleanSteps(p[@"fwd"])) [e.fwd addObject:[s mutableCopy]];
    for (NSArray *s in CleanSteps(p[@"rev"])) [e.rev addObject:[s mutableCopy]];
    if (!p) [e startFrom:0 announce:NO];        // a new pattern starts as a copy of a real one
    sEditor = e;
    [e build];
}

- (void)startFrom:(int)index announce:(BOOL)announce {
    self.pattern[@"base"] = @(index);
    NSDictionary *r = BFOilReady() ? BFOilCompute(index, nil, nil, 0, NO) : nil;
    self.pattern[@"exact"] = @(r != nil);              // keep the game's own step distances
    int tdrop = [r[@"tdrop"] intValue];
    if (tdrop > 0) self.pattern[@"drop"] = @(tdrop);   // the start pattern's reverse brush drop
    [self.dropStepper setValue:[self.pattern[@"drop"] intValue]];
    [self.fwd removeAllObjects];
    [self.rev removeAllObjects];
    for (NSArray *s in CleanSteps(r[@"fwd"])) [self.fwd addObject:[s mutableCopy]];
    for (NSArray *s in CleanSteps(r[@"rev"])) [self.rev addObject:[s mutableCopy]];
    if (!self.fwd.count) {                        // game not loaded yet: a simple house-style start
        for (NSArray *s in @[ @[@2, @38, @2, @14, @0], @[@4, @36, @3, @16, @0], @[@7, @33, @4, @18, @0], @[@10, @30, @3, @20, @0], @[@2, @38, @0, @26, @40] ])
            [self.fwd addObject:[s mutableCopy]];
        for (NSArray *s in @[ @[@8, @32, @3, @20, @0], @[@5, @35, @2, @18, @0] ]) [self.rev addObject:[s mutableCopy]];
    }
    if (announce) { [self rebuildSteps]; [self schedulePreview]; }
}

- (void)build {
    UIView *overlay, *card;
    UIStackView *stack = OSheet(&overlay, &card, NO);
    self.overlay = overlay;
    self.card = card;

    UIButton *cancel = OButton(@"Cancel", NO, self, @selector(cancel));
    UIButton *save = OButton(@"Save", YES, self, @selector(save));
    UILabel *title = OLabel(@"Oil pattern", 18, UIFontWeightHeavy, UIColor.whiteColor);
    title.textAlignment = NSTextAlignmentCenter;
    UIStackView *head = OStack(@[cancel, title, save], UILayoutConstraintAxisHorizontal, 8);
    head.distribution = UIStackViewDistributionEqualCentering;
    [stack addArrangedSubview:head];

    UITextField *f = [UITextField new];
    f.text = self.pattern[@"name"] ?: @"My pattern";
    f.font = OFont(17, UIFontWeightBold);
    f.textColor = UIColor.whiteColor;
    f.backgroundColor = ODim(0.08);
    f.layer.cornerRadius = 12;
    f.leftView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 12, 10)];
    f.leftViewMode = UITextFieldViewModeAlways;
    f.clearButtonMode = UITextFieldViewModeWhileEditing;
    f.returnKeyType = UIReturnKeyDone;
    f.keyboardAppearance = UIKeyboardAppearanceDark;
    f.delegate = self;
    [f.heightAnchor constraintEqualToConstant:44].active = YES;
    self.nameField = f;
    [stack addArrangedSubview:f];

    // machine settings + starting steps from a real pattern
    self.baseButton = OButton(@"", NO, nil, nil);
    self.baseButton.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
    [self.baseButton addTarget:self action:@selector(pickStart) forControlEvents:UIControlEventTouchUpInside];
    [self refreshBaseMenu];
    [stack addArrangedSubview:self.baseButton];

    // the sheet's "Reverse Brush Drop": the engine only lays reverse oil short of it
    BFOilStepper *drop = [[BFOilStepper alloc] initWithTitle:@"REVERSE BRUSH DROP" min:0 max:60];
    drop.format = ^NSString *(int v) { return v > 0 ? [NSString stringWithFormat:@"%d ft", v] : @"from start pattern"; };
    drop.value = [self.pattern[@"drop"] intValue];
    __weak BFOilEditor *weakSelf = self;
    drop.changed = ^(int v) { weakSelf.pattern[@"drop"] = @(v); [weakSelf schedulePreview]; };
    self.dropStepper = drop;
    [stack addArrangedSubview:drop];

    self.preview = [UIImageView new];
    self.preview.layer.cornerRadius = 10;
    self.preview.clipsToBounds = YES;
    self.preview.backgroundColor = [UIColor colorWithRed:0.86 green:0.70 blue:0.50 alpha:1];
    [self.preview.heightAnchor constraintEqualToConstant:96].active = YES;
    [stack addArrangedSubview:self.preview];
    UIStackView *axis = OStack(@[OLabel(@"Foul line", 10, UIFontWeightSemibold, ODim(0.45)), [UIView new],
                                 OLabel(@"Arrows", 10, UIFontWeightSemibold, ODim(0.45)), [UIView new],
                                 OLabel(@"Pins \u2192", 10, UIFontWeightSemibold, ODim(0.45))], UILayoutConstraintAxisHorizontal, 4);
    axis.distribution = UIStackViewDistributionEqualSpacing;
    [stack addArrangedSubview:axis];
    [stack addArrangedSubview:OLegend()];
    self.stats = OLabel(@"", 13, UIFontWeightSemibold, OYellow());
    [stack addArrangedSubview:self.stats];

    self.fwdStack = OStack(nil, UILayoutConstraintAxisVertical, 8);
    self.revStack = OStack(nil, UILayoutConstraintAxisVertical, 8);
    [stack addArrangedSubview:[self tab:@"Forward oil"]];
    [stack addArrangedSubview:self.fwdStack];
    [stack addArrangedSubview:OButton(@"+ Add forward step", NO, self, @selector(addFwd))];
    [stack addArrangedSubview:[self tab:@"Reverse oil"]];
    [stack addArrangedSubview:self.revStack];
    [stack addArrangedSubview:OButton(@"+ Add reverse step", NO, self, @selector(addRev))];
    [stack addArrangedSubview:OLabel(@"Boards: 2L is the 2nd board from the left, 2R the 2nd from the right. Like a real Kegel machine, each step's distance comes from loads \u00D7 speed (0 loads = travel without oil), and colors show how thick the oil is.",
                                     12, UIFontWeightRegular, ODim(0.5))];
    [self rebuildSteps];
    OPresent(overlay, card, NO);
    [self schedulePreview];
}

- (UIView *)tab:(NSString *)text {               // the game's yellow tab look
    UILabel *l = OLabel(text.uppercaseString, 13, UIFontWeightHeavy, [UIColor colorWithWhite:0.1 alpha:1]);
    l.textAlignment = NSTextAlignmentCenter;
    l.backgroundColor = OYellow();
    l.layer.cornerRadius = 9;
    l.clipsToBounds = YES;
    [l.heightAnchor constraintEqualToConstant:30].active = YES;
    return l;
}

- (void)refreshBaseMenu {
    NSString *from = self.pattern[@"from"];
    if (!from.length) {
        int base = [self.pattern[@"base"] intValue];
        for (NSDictionary *b in (BFOilReady() ? BFOilBuiltins() : @[])) if ([b[@"index"] intValue] == base) from = b[@"name"];
    }
    [self.baseButton setTitle:[NSString stringWithFormat:@"Start from: %@  \u25BE", from ?: @"a game pattern"] forState:UIControlStateNormal];
}

// our own picker (iOS pop-up menus don't open inside the game)
- (void)pickStart {
    __weak BFOilEditor *weak = self;
    NSMutableArray *items = [NSMutableArray array];
    for (NSDictionary *c in BFOilCollection())
        [items addObject:@{ @"title": [NSString stringWithFormat:@"\u2605 %@ \u00B7 %@ ft", c[@"name"], c[@"feet"]], @"action": ^{ [weak startFromPattern:c]; } }];
    for (NSDictionary *b in (BFOilReady() ? BFOilBuiltins() : @[])) {
        int idx = [b[@"index"] intValue];
        NSString *name = b[@"name"];
        [items addObject:@{ @"title": [NSString stringWithFormat:@"%@ \u00B7 %@ ft", name, b[@"feet"]], @"action": ^{
            [weak startFrom:idx announce:YES];
            weak.pattern[@"from"] = name;
            [weak refreshBaseMenu];
        } }];
    }
    if (!items.count) { OToast(@"Open a game first so the patterns are loaded"); return; }
    OChoose(@"Start from", @"Replaces your steps with a copy of this pattern's steps, so you can tweak it.", items);
}

- (void)startFromPattern:(NSDictionary *)c {
    self.pattern[@"base"] = c[@"base"] ?: @0;
    self.pattern[@"exact"] = c[@"exact"] ?: @NO;
    self.pattern[@"drop"] = c[@"drop"] ?: @0;
    [self.dropStepper setValue:[c[@"drop"] intValue]];
    self.pattern[@"from"] = c[@"name"];
    [self.fwd removeAllObjects];
    [self.rev removeAllObjects];
    for (NSArray *st in CleanSteps(c[@"fwd"])) [self.fwd addObject:[st mutableCopy]];
    for (NSArray *st in CleanSteps(c[@"rev"])) [self.rev addObject:[st mutableCopy]];
    [self rebuildSteps];
    [self schedulePreview];
    [self refreshBaseMenu];
}

- (void)rebuildSteps {
    self.fwdEnds = [NSMutableArray array];
    self.revEnds = [NSMutableArray array];
    for (UIStackView *s in @[self.fwdStack, self.revStack]) for (UIView *v in s.arrangedSubviews) [v removeFromSuperview];
    for (NSUInteger i = 0; i < self.fwd.count; i++) [self.fwdStack addArrangedSubview:[self stepRow:self.fwd index:i ends:self.fwdEnds]];
    for (NSUInteger i = 0; i < self.rev.count; i++) [self.revStack addArrangedSubview:[self stepRow:self.rev index:i ends:self.revEnds]];
}

- (UIView *)stepRow:(NSMutableArray<NSMutableArray<NSNumber *> *> *)list index:(NSUInteger)i ends:(NSMutableArray *)ends {
    NSMutableArray<NSNumber *> *s = list[i];
    UIView *box = [UIView new];
    box.backgroundColor = ODim(0.04);
    box.layer.cornerRadius = 12;
    UILabel *name = OLabel([NSString stringWithFormat:@"Step %lu", (unsigned long)i + 1], 13, UIFontWeightHeavy, UIColor.whiteColor);
    UILabel *end = OLabel(@"", 12, UIFontWeightSemibold, ODim(0.55));
    [ends addObject:end];
    UIButton *del = [UIButton buttonWithType:UIButtonTypeSystem];
    [del setTitle:@"\u2715" forState:UIControlStateNormal];
    [del setTitleColor:ODim(0.5) forState:UIControlStateNormal];
    del.titleLabel.font = OFont(16, UIFontWeightBold);
    __weak BFOilEditor *weak = self;
    [del addAction:[UIAction actionWithHandler:^(UIAction *a) {
        [list removeObjectAtIndex:i];
        weak.pattern[@"exact"] = @NO;
        [weak rebuildSteps];
        [weak schedulePreview];
    }] forControlEvents:UIControlEventTouchUpInside];
    UIStackView *top = OStack(@[name, end, [UIView new], del], UILayoutConstraintAxisHorizontal, 8);

    while (s.count < 5) [s addObject:@0];
    NSString *titles[5] = { @"START", @"STOP", @"LOADS", @"SPEED in/s", @"TRAVEL TO (no oil)" };
    int lo[5] = { 1, 1, 0, 6, 0 }, hi[5] = { 39, 39, 99, 30, 70 };
    BOOL forward = list == self.fwd;
    NSMutableArray<BFOilStepper *> *steppers = [NSMutableArray array];
    for (int k = 0; k < 5; k++) {
        BFOilStepper *st = [[BFOilStepper alloc] initWithTitle:titles[k] min:lo[k] max:hi[k]];
        if (k < 2) st.format = ^NSString *(int v) { return BoardLabel(v); };
        if (k == 4) st.format = ^NSString *(int v) { return [NSString stringWithFormat:@"%d ft", v]; };
        st.value = k == 4 ? (int)lroundf(s[4].floatValue) : s[k].intValue;
        [steppers addObject:st];
    }
    BFOilStepper *travel = steppers[4];
    travel.hidden = s[2].intValue != 0;
    for (int k = 0; k < 5; k++) {
        steppers[k].changed = ^(int v) {
            s[k] = @(v);
            if (k == 2 || k == 3) weak.pattern[@"exact"] = @NO;   // distances now come from loads x speed
            if (k == 2) {                              // zero loads = machine travels without oil
                travel.hidden = v != 0;
                if (v == 0 && forward && s[4].floatValue < 1) { s[4] = @(40); travel.value = 40; }
            }
            [weak schedulePreview];
        };
    }
    UIStackView *r1 = OStack(@[steppers[0], steppers[1]], UILayoutConstraintAxisHorizontal, 8);
    UIStackView *r2 = OStack(@[steppers[2], steppers[3]], UILayoutConstraintAxisHorizontal, 8);
    r1.distribution = r2.distribution = UIStackViewDistributionFillEqually;
    UIStackView *col = OStack(@[top, r1, r2, travel], UILayoutConstraintAxisVertical, 8);
    col.translatesAutoresizingMaskIntoConstraints = NO;
    [box addSubview:col];
    [NSLayoutConstraint activateConstraints:@[
        [col.topAnchor constraintEqualToAnchor:box.topAnchor constant:10],
        [col.bottomAnchor constraintEqualToAnchor:box.bottomAnchor constant:-10],
        [col.leadingAnchor constraintEqualToAnchor:box.leadingAnchor constant:10],
        [col.trailingAnchor constraintEqualToAnchor:box.trailingAnchor constant:-10],
    ]];
    return box;
}

- (void)addFwd {
    self.pattern[@"exact"] = @NO;
    NSArray *last = self.fwd.lastObject ?: @[@10, @30, @2, @16, @0];
    if (self.fwd.count < 40) [self.fwd addObject:[last mutableCopy]];
    [self rebuildSteps];
    [self schedulePreview];
}
- (void)addRev {
    self.pattern[@"exact"] = @NO;
    NSArray *last = self.rev.lastObject ?: @[@8, @32, @2, @18, @0];
    if (self.rev.count < 40) [self.rev addObject:[last mutableCopy]];
    [self rebuildSteps];
    [self schedulePreview];
}

- (void)schedulePreview {
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(updatePreview) object:nil];
    [self performSelector:@selector(updatePreview) withObject:nil afterDelay:0.15];
}

- (void)updatePreview {
    if (!BFOilReady()) {
        self.stats.text = @"Preview shows up once the game has loaded a lane.";
        return;
    }
    NSDictionary *r = BFOilCompute([self.pattern[@"base"] intValue], self.fwd, self.rev, [self.pattern[@"drop"] intValue],
                                   [self.pattern[@"exact"] boolValue]);
    if (!r) {
        self.stats.text = @"The game couldn't build this pattern. Try fewer or smaller steps.";
        return;
    }
    [self.preview layoutIfNeeded];
    CGSize sz = self.preview.bounds.size;
    if (sz.width < 10) sz = CGSizeMake(360, 96);
    self.preview.image = RenderPreview(r, sz);
    float far = 0;
    NSArray *fe = r[@"fwd"], *re = r[@"rev"];
    for (NSUInteger i = 0; i < self.fwdEnds.count && i < fe.count; i++) {
        float ft = [fe[i][4] floatValue];
        far = fmaxf(far, ft);
        self.fwdEnds[i].text = [NSString stringWithFormat:@"to %.1f ft", ft];
    }
    for (NSUInteger i = 0; i < self.revEnds.count && i < re.count; i++)
        self.revEnds[i].text = [NSString stringWithFormat:@"back to %.1f ft", [re[i][4] floatValue]];
    int loads = 0;
    for (NSArray *s in self.fwd) loads += [s[2] intValue];
    for (NSArray *s in self.rev) loads += [s[2] intValue];
    self.stats.text = [NSString stringWithFormat:@"Oil out to %.0f ft \u00B7 %d loads \u00B7 %lu forward, %lu reverse steps",
                       far, loads, (unsigned long)self.fwd.count, (unsigned long)self.rev.count];
}

- (BOOL)textFieldShouldReturn:(UITextField *)t { [t resignFirstResponder]; return YES; }

- (void)cancel {
    ODismiss(self.overlay);
    sEditor = nil;
}

- (void)save {
    NSString *name = [self.nameField.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    self.pattern[@"name"] = name.length ? [name substringToIndex:MIN((NSUInteger)40, name.length)] : @"My pattern";
    self.pattern[@"fwd"] = CleanSteps(self.fwd);
    self.pattern[@"rev"] = CleanSteps(self.rev);
    NSDictionary *saved = [self.pattern copy];
    UpsertPattern(saved);
    ODismiss(self.overlay);
    if (self.onDone) self.onDone(saved);
    sEditor = nil;
}
@end

static void ImportKegelFile(void (^done)(NSDictionary *pattern, NSString *error));

#pragma mark - pattern library (+ QR share / import)

@interface BFOilLibrary : NSObject <PHPickerViewControllerDelegate, AVCaptureMetadataOutputObjectsDelegate, UIGestureRecognizerDelegate>
@property (nonatomic, strong) UIView *overlay, *card, *scanOverlay;
@property (nonatomic, strong) UIStackView *list;
@property (nonatomic, strong) UILabel *status;
@property (nonatomic, strong) AVCaptureSession *session;
@property (nonatomic, strong) NSCache *thumbs;
@property (nonatomic) BOOL showing;
+ (instancetype)shared;
- (void)show;
@end

@implementation BFOilLibrary

+ (instancetype)shared {
    static BFOilLibrary *l;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ l = [BFOilLibrary new]; l.thumbs = [NSCache new]; });
    return l;
}

- (void)show {
    if (self.showing) return;
    UIView *overlay, *card;
    UIStackView *stack = OSheet(&overlay, &card, YES);
    self.overlay = overlay;
    self.card = card;
    UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(outsideTap:)];
    tap.cancelsTouchesInView = NO;
    [overlay addGestureRecognizer:tap];
    UISwipeGestureRecognizer *up = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(hide)];
    up.direction = UISwipeGestureRecognizerDirectionUp;
    [card addGestureRecognizer:up];

    UIButton *close = [UIButton buttonWithType:UIButtonTypeSystem];
    [close setTitle:@"\u2715" forState:UIControlStateNormal];
    [close setTitleColor:ODim(0.7) forState:UIControlStateNormal];
    close.titleLabel.font = OFont(20, UIFontWeightBold);
    [close addTarget:self action:@selector(hide) forControlEvents:UIControlEventTouchUpInside];
    [stack addArrangedSubview:OStack(@[OLabel(@"Custom oil", 22, UIFontWeightHeavy, UIColor.whiteColor), [UIView new], close], UILayoutConstraintAxisHorizontal, 8)];
    self.status = OLabel(@"", 13, UIFontWeightSemibold, OYellow());
    [stack addArrangedSubview:self.status];
    [stack addArrangedSubview:OLabel(@"Pick a pattern, then start practice with any pattern in the game's list: your custom oil replaces it on the lane. Practice only. Tap the \u22EF button on a pattern to edit, share, duplicate or delete it.",
                                     12, UIFontWeightRegular, ODim(0.55))];
    [stack addArrangedSubview:OLegend()];
    self.list = OStack(nil, UILayoutConstraintAxisVertical, 8);
    [stack addArrangedSubview:self.list];
    UIButton *add = OButton(@"+ New pattern", YES, self, @selector(newPattern));
    [stack addArrangedSubview:add];
    UIStackView *imports = OStack(@[OButton(@"Scan QR", NO, self, @selector(scan)), OButton(@"From photo", NO, self, @selector(photo)),
                                    OButton(@"Paste code", NO, self, @selector(paste))], UILayoutConstraintAxisHorizontal, 8);
    imports.distribution = UIStackViewDistributionFillEqually;
    [stack addArrangedSubview:imports];
    [stack addArrangedSubview:OButton(@"\U0001F4C2  Import a Kegel pattern file", NO, self, @selector(kegelFile))];
    [stack addArrangedSubview:OLabel(@"From the Kegel Pattern Library app or website: pick the downloaded pattern sheet (.pdf) or .zip (or the .Pattern / .txt inside). Imports the steps, distances and drop brush.",
                                     11, UIFontWeightRegular, ODim(0.45))];
    [self reload];
    self.showing = YES;
    OPresent(overlay, card, YES);
}

- (void)outsideTap:(UITapGestureRecognizer *)g {
    if (![self.card pointInside:[g locationInView:self.card] withEvent:nil]) [self hide];
}

- (void)hide {
    if (!self.showing) return;
    self.showing = NO;
    ODismiss(self.overlay);
}

- (UIImage *)thumbFor:(NSDictionary *)p {
    NSString *key = [NSString stringWithFormat:@"%@|%@|%@|%@|%@|%@", p[@"id"], p[@"base"], p[@"drop"], p[@"exact"], p[@"fwd"], p[@"rev"]];
    UIImage *img = [self.thumbs objectForKey:key];
    if (img || !BFOilReady()) return img;
    NSDictionary *r = BFOilCompute([p[@"base"] intValue], p[@"fwd"], p[@"rev"], [p[@"drop"] intValue], [p[@"exact"] boolValue]);
    img = r ? RenderPreview(r, CGSizeMake(120, 34)) : nil;
    if (img) [self.thumbs setObject:img forKey:key];
    return img;
}

- (UIView *)row:(NSDictionary *)p {
    BOOL active = p ? [p[@"id"] isEqualToString:ActiveId()] : ActiveId() == nil || !PatternById(ActiveId());
    UIView *box = [UIView new];
    box.backgroundColor = active ? [OYellow() colorWithAlphaComponent:0.16] : ODim(0.05);
    box.layer.cornerRadius = 14;
    box.layer.borderWidth = active ? 1.5 : 0;
    box.layer.borderColor = OYellow().CGColor;
    UILabel *dot = OLabel(active ? @"\u25C9" : @"\u25CB", 20, UIFontWeightBold, active ? OYellow() : ODim(0.4));
    UIImageView *thumb = [UIImageView new];
    thumb.layer.cornerRadius = 6;
    thumb.clipsToBounds = YES;
    thumb.backgroundColor = [UIColor colorWithRed:0.86 green:0.70 blue:0.50 alpha:1];
    thumb.image = p ? [self thumbFor:p] : nil;
    [thumb.widthAnchor constraintEqualToConstant:p ? 96 : 0].active = YES;
    [thumb.heightAnchor constraintEqualToConstant:30].active = YES;
    thumb.hidden = !p;
    int loads = 0;
    for (NSArray *s in p[@"fwd"]) loads += [s[2] intValue];
    for (NSArray *s in p[@"rev"]) loads += [s[2] intValue];
    UILabel *name = OLabel(p ? p[@"name"] : @"Off: the game's pattern", 15, UIFontWeightBold, UIColor.whiteColor);
    NSString *subText = !p ? @"No custom oil"
        : [p[@"collection"] boolValue] ? [NSString stringWithFormat:@"%@ \u00B7 %@ ft \u00B7 %@ mL", p[@"event"], p[@"feet"], p[@"ml"]]
        : [NSString stringWithFormat:@"%lu forward \u00B7 %lu reverse \u00B7 %d loads", (unsigned long)[p[@"fwd"] count], (unsigned long)[p[@"rev"] count], loads];
    UILabel *sub = OLabel(subText, 11, UIFontWeightSemibold, ODim(0.5));
    UIStackView *texts = OStack(@[name, sub], UILayoutConstraintAxisVertical, 1);
    NSMutableArray *views = [NSMutableArray arrayWithObjects:dot, thumb, texts, nil];
    if (p) {
        UIButton *more = [UIButton buttonWithType:UIButtonTypeSystem];
        [more setImage:[UIImage systemImageNamed:@"ellipsis.circle" withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:24 weight:UIImageSymbolWeightSemibold]] forState:UIControlStateNormal];
        more.tintColor = OYellow();
        more.accessibilityLabel = @"Edit, share, duplicate or delete";
        [more.widthAnchor constraintEqualToConstant:44].active = YES;
        [more.heightAnchor constraintEqualToConstant:44].active = YES;
        objc_setAssociatedObject(more, "bpPattern", p, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [more addTarget:self action:@selector(moreTapped:) forControlEvents:UIControlEventTouchUpInside];
        [views addObject:more];
    }
    UIStackView *row = OStack(views, UILayoutConstraintAxisHorizontal, 10);
    row.translatesAutoresizingMaskIntoConstraints = NO;
    [box addSubview:row];
    [NSLayoutConstraint activateConstraints:@[
        [row.topAnchor constraintEqualToAnchor:box.topAnchor constant:10],
        [row.bottomAnchor constraintEqualToAnchor:box.bottomAnchor constant:-10],
        [row.leadingAnchor constraintEqualToAnchor:box.leadingAnchor constant:12],
        [row.trailingAnchor constraintEqualToAnchor:box.trailingAnchor constant:-6],
    ]];
    UITapGestureRecognizer *pick = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(pickRow:)];
    pick.delegate = self;                          // so taps on the menu button reach the button
    [box addGestureRecognizer:pick];
    box.accessibilityIdentifier = p[@"id"] ?: @"";
    return box;
}

- (void)moreTapped:(UIButton *)b {
    NSDictionary *p = objc_getAssociatedObject(b, "bpPattern");
    if (!p) return;
    __weak BFOilLibrary *weak = self;
    BOOL builtIn = [p[@"collection"] boolValue];
    NSMutableArray *items = [NSMutableArray array];
    if (!builtIn) [items addObject:@{ @"title": @"\u270E  Edit", @"action": ^{
        [BFOilEditor editPattern:p done:^(NSDictionary *s) { [weak reload]; }];
    } }];
    [items addObject:@{ @"title": @"\u2B06\uFE0E  Share / export QR code", @"action": ^{ [weak share:p]; } }];
    [items addObject:@{ @"title": @"\u2398  Copy code", @"action": ^{
        UIPasteboard.generalPasteboard.string = EncodePattern(p);
        OToast(@"Code copied");
    } }];
    [items addObject:@{ @"title": builtIn ? @"\u29C9  Make an editable copy" : @"\u29C9  Duplicate", @"action": ^{
        NSMutableDictionary *c = [p mutableCopy];
        c[@"id"] = NSUUID.UUID.UUIDString;
        c[@"name"] = builtIn ? [NSString stringWithFormat:@"%@ (my copy)", p[@"name"]] : [NSString stringWithFormat:@"%@ copy", p[@"name"]];
        [c removeObjectForKey:@"collection"];
        UpsertPattern(c);
        [weak reload];
        if (builtIn) [BFOilEditor editPattern:c done:^(NSDictionary *s) { [weak reload]; }];
    } }];
    if (!builtIn) [items addObject:@{ @"title": @"\u2715  Delete", @"destructive": @YES, @"confirm": @YES, @"action": ^{ [weak remove:p]; } }];
    OChoose(p[@"name"], builtIn ? [NSString stringWithFormat:@"BowlingPlus collection \u00B7 %@", p[@"event"]] : nil, items);
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)g shouldReceiveTouch:(UITouch *)touch {
    for (UIView *v = touch.view; v && v != g.view; v = v.superview)
        if ([v isKindOfClass:[UIControl class]]) return NO;
    return YES;
}

- (void)pickRow:(UITapGestureRecognizer *)g {
    NSString *pid = g.view.accessibilityIdentifier;
    SetActiveId(pid.length ? pid : nil);
    [[UIImpactFeedbackGenerator new] impactOccurred];
    [self reload];
}

- (void)reload {
    for (UIView *v in self.list.arrangedSubviews) [v removeFromSuperview];
    [self.list addArrangedSubview:[self row:nil]];
    [self.list addArrangedSubview:OLabel(@"BOWLINGPLUS COLLECTION", 11, UIFontWeightHeavy, ODim(0.45))];
    for (NSDictionary *p in BFOilCollection()) [self.list addArrangedSubview:[self row:p]];
    [self.list addArrangedSubview:OLabel(@"MY PATTERNS", 11, UIFontWeightHeavy, ODim(0.45))];
    NSArray *mine = LoadPatterns();
    for (NSDictionary *p in mine) [self.list addArrangedSubview:[self row:p]];
    if (!mine.count) [self.list addArrangedSubview:OLabel(@"None yet. Tap + New pattern, or import one with a QR code.", 12, UIFontWeightRegular, ODim(0.45))];
    NSDictionary *act = PatternById(ActiveId());
    NSString *st = BFOilStatusLine();
    self.status.text = act ? [NSString stringWithFormat:@"On the lane in practice: %@", act[@"name"]]
                           : (st.length ? st : @"Using the game's patterns");
}

- (void)newPattern {
    __weak BFOilLibrary *weak = self;
    [BFOilEditor editPattern:nil done:^(NSDictionary *s) {
        SetActiveId(s[@"id"]);
        [weak reload];
    }];
}

- (void)remove:(NSDictionary *)p {
    NSMutableArray *all = LoadPatterns();
    [all filterUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSDictionary *q, NSDictionary *b) { return ![q[@"id"] isEqualToString:p[@"id"]]; }]];
    SavePatterns(all);
    if ([p[@"id"] isEqualToString:ActiveId()]) SetActiveId(nil);
    OToast([NSString stringWithFormat:@"Deleted \"%@\"", p[@"name"]]);
    [self reload];
}

#pragma mark share

- (void)share:(NSDictionary *)p {
    NSString *code = EncodePattern(p);
    CIFilter *f = [CIFilter filterWithName:@"CIQRCodeGenerator"];
    [f setValue:[code dataUsingEncoding:NSUTF8StringEncoding] forKey:@"inputMessage"];
    [f setValue:@"M" forKey:@"inputCorrectionLevel"];
    CIImage *qr = [f.outputImage imageByApplyingTransform:CGAffineTransformMakeScale(12, 12)];
    CGImageRef cg = qr ? [[CIContext context] createCGImage:qr fromRect:qr.extent] : NULL;
    UIImage *img = cg ? [UIImage imageWithCGImage:cg] : nil;
    if (cg) CGImageRelease(cg);
    UIView *overlay, *card;
    UIStackView *stack = OSheet(&overlay, &card, NO);
    UILabel *t = OLabel(p[@"name"], 20, UIFontWeightHeavy, UIColor.whiteColor);
    t.textAlignment = NSTextAlignmentCenter;
    [stack addArrangedSubview:t];
    UIImageView *iv = [[UIImageView alloc] initWithImage:img];
    iv.contentMode = UIViewContentModeScaleAspectFit;
    iv.backgroundColor = UIColor.whiteColor;
    iv.layer.cornerRadius = 14;
    iv.clipsToBounds = YES;
    [iv.heightAnchor constraintEqualToConstant:260].active = YES;
    [stack addArrangedSubview:iv];
    [stack addArrangedSubview:OLabel(@"Friends with BowlingPlus: shake \u2192 Custom oil \u2192 Scan QR (or From photo with a screenshot).", 12, UIFontWeightRegular, ODim(0.55))];
    UIButton *shareB = OButton(@"Share", YES, nil, nil), *copyB = OButton(@"Copy code", NO, nil, nil), *done = OButton(@"Done", NO, nil, nil);
    [shareB addAction:[UIAction actionWithHandler:^(UIAction *a) {
        NSMutableArray *items = [NSMutableArray arrayWithObject:code];
        if (img) [items insertObject:img atIndex:0];
        UIActivityViewController *avc = [[UIActivityViewController alloc] initWithActivityItems:items applicationActivities:nil];
        avc.completionWithItemsHandler = ^(UIActivityType t, BOOL done, NSArray *r, NSError *e) { ODonePresenting(); };
        OPresentVC(avc);
    }] forControlEvents:UIControlEventTouchUpInside];
    [copyB addAction:[UIAction actionWithHandler:^(UIAction *a) {
        UIPasteboard.generalPasteboard.string = code;
        OToast(@"Code copied");
    }] forControlEvents:UIControlEventTouchUpInside];
    [done addAction:[UIAction actionWithHandler:^(UIAction *a) { ODismiss(overlay); }] forControlEvents:UIControlEventTouchUpInside];
    UIStackView *btns = OStack(@[shareB, copyB, done], UILayoutConstraintAxisHorizontal, 8);
    btns.distribution = UIStackViewDistributionFillEqually;
    [stack addArrangedSubview:btns];
    OPresent(overlay, card, NO);
}

#pragma mark import

- (void)importCode:(NSString *)code {
    NSDictionary *p = DecodePattern(code);
    if (!p) { OToast(@"That isn't a BowlingPlus oil pattern code"); return; }
    UpsertPattern(p);
    OToast([NSString stringWithFormat:@"Imported \"%@\"", p[@"name"]]);
    [self reload];
}

- (void)paste { [self importCode:UIPasteboard.generalPasteboard.string]; }

- (void)kegelFile {
    __weak BFOilLibrary *weak = self;
    ImportKegelFile(^(NSDictionary *p, NSString *error) {
        if (!p) { OToast(error); return; }
        UpsertPattern(p);
        SetActiveId(p[@"id"]);
        OToast([NSString stringWithFormat:@"Imported \"%@\" from Kegel", p[@"name"]]);
        [weak reload];
    });
}

- (void)photo {
    PHPickerConfiguration *cfg = [PHPickerConfiguration new];
    cfg.filter = [PHPickerFilter imagesFilter];
    cfg.selectionLimit = 1;
    PHPickerViewController *picker = [[PHPickerViewController alloc] initWithConfiguration:cfg];
    picker.delegate = self;
    OPresentVC(picker);
}

- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results {
    [picker dismissViewControllerAnimated:YES completion:^{ ODonePresenting(); }];
    NSItemProvider *ip = results.firstObject.itemProvider;
    if (![ip canLoadObjectOfClass:[UIImage class]]) return;
    [ip loadObjectOfClass:[UIImage class] completionHandler:^(id<NSItemProviderReading> obj, NSError *err) {
        UIImage *img = (UIImage *)obj;
        CIImage *ci = img.CGImage ? [CIImage imageWithCGImage:img.CGImage] : nil;
        CIDetector *d = [CIDetector detectorOfType:CIDetectorTypeQRCode context:nil options:@{ CIDetectorAccuracy: CIDetectorAccuracyHigh }];
        NSString *found = nil;
        for (CIFeature *f in (ci ? [d featuresInImage:ci] : @[]))
            if ([f isKindOfClass:[CIQRCodeFeature class]] && [((CIQRCodeFeature *)f).messageString hasPrefix:kCodePrefix]) found = ((CIQRCodeFeature *)f).messageString;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (found) [self importCode:found]; else OToast(@"No oil pattern QR code in that picture");
        });
    }];
}

- (void)scan {
    AVAuthorizationStatus st = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (st == AVAuthorizationStatusNotDetermined) {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
            dispatch_async(dispatch_get_main_queue(), ^{ if (granted) [self scan]; });
        }];
        return;
    }
    if (st != AVAuthorizationStatusAuthorized) { OToast(@"Camera access is off for this app (Settings)"); return; }
    AVCaptureDevice *cam = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    AVCaptureDeviceInput *in = cam ? [AVCaptureDeviceInput deviceInputWithDevice:cam error:nil] : nil;
    if (!in) { OToast(@"No camera available"); return; }
    AVCaptureSession *s = [AVCaptureSession new];
    AVCaptureMetadataOutput *out = [AVCaptureMetadataOutput new];
    if (![s canAddInput:in] || ![s canAddOutput:out]) return;
    [s addInput:in];
    [s addOutput:out];
    [out setMetadataObjectsDelegate:self queue:dispatch_get_main_queue()];
    if ([out.availableMetadataObjectTypes containsObject:AVMetadataObjectTypeQRCode]) out.metadataObjectTypes = @[AVMetadataObjectTypeQRCode];
    UIWindow *w = OHostWindow();
    UIView *ov = [[UIView alloc] initWithFrame:w.bounds];
    ov.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    ov.backgroundColor = UIColor.blackColor;
    AVCaptureVideoPreviewLayer *pl = [AVCaptureVideoPreviewLayer layerWithSession:s];
    pl.frame = ov.bounds;
    pl.videoGravity = AVLayerVideoGravityResizeAspectFill;
    [ov.layer addSublayer:pl];
    UILabel *hint = OLabel(@"Point at an oil pattern QR code", 17, UIFontWeightBold, UIColor.whiteColor);
    hint.textAlignment = NSTextAlignmentCenter;
    UIButton *cancel = OButton(@"Cancel", YES, self, @selector(stopScan));
    UIStackView *bar = OStack(@[hint, cancel], UILayoutConstraintAxisVertical, 12);
    bar.translatesAutoresizingMaskIntoConstraints = NO;
    [ov addSubview:bar];
    [NSLayoutConstraint activateConstraints:@[
        [bar.leadingAnchor constraintEqualToAnchor:ov.leadingAnchor constant:24],
        [bar.trailingAnchor constraintEqualToAnchor:ov.trailingAnchor constant:-24],
        [bar.bottomAnchor constraintEqualToAnchor:ov.safeAreaLayoutGuide.bottomAnchor constant:-24],
    ]];
    [w addSubview:ov];
    self.scanOverlay = ov;
    self.session = s;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{ [s startRunning]; });
}

- (void)stopScan {
    AVCaptureSession *s = self.session;
    self.session = nil;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{ [s stopRunning]; });
    [self.scanOverlay removeFromSuperview];
    self.scanOverlay = nil;
}

- (void)captureOutput:(AVCaptureOutput *)output didOutputMetadataObjects:(NSArray<__kindof AVMetadataObject *> *)objects fromConnection:(AVCaptureConnection *)c {
    for (AVMetadataObject *o in objects) {
        if (![o isKindOfClass:[AVMetadataMachineReadableCodeObject class]]) continue;
        NSString *v = ((AVMetadataMachineReadableCodeObject *)o).stringValue;
        if (![v hasPrefix:kCodePrefix] || !self.session) continue;
        [[UINotificationFeedbackGenerator new] notificationOccurred:UINotificationFeedbackTypeSuccess];
        [self stopScan];
        [self importCode:v];
        return;
    }
}
@end

#pragma mark - the pull-down tab on the practice pattern screen

@interface BFOilTab : NSObject
@property (nonatomic, strong) UIButton *tab;
@property (nonatomic, strong) NSTimer *timer;
@end

@implementation BFOilTab
- (void)tick {
    UIWindow *w = OHostWindow();
    BOOL show = w && BFPracticeLobbyOpen() && ![BFOilLibrary shared].showing && !sEditor;
    if (!show) { [self.tab removeFromSuperview]; return; }
    if (!self.tab) {
        UIButton *b = OButton(@"\U0001F6E2 Custom oil \u25BE", YES, self, @selector(open));
        b.titleLabel.font = OFont(14, UIFontWeightHeavy);
        b.contentEdgeInsets = UIEdgeInsetsMake(7, 16, 7, 16);
        b.layer.cornerRadius = 16;
        b.layer.shadowColor = UIColor.blackColor.CGColor;
        b.layer.shadowOpacity = 0.4;
        b.layer.shadowRadius = 6;
        b.layer.shadowOffset = CGSizeMake(0, 3);
        b.translatesAutoresizingMaskIntoConstraints = NO;
        UISwipeGestureRecognizer *down = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(open)];
        down.direction = UISwipeGestureRecognizerDirectionDown;
        [b addGestureRecognizer:down];
        self.tab = b;
    }
    NSDictionary *act = PatternById(ActiveId());
    [self.tab setTitle:act ? [NSString stringWithFormat:@"\U0001F6E2 %@ \u25BE", act[@"name"]] : @"\U0001F6E2 Custom oil \u25BE" forState:UIControlStateNormal];
    if (self.tab.superview != w) {
        [self.tab removeFromSuperview];
        [w addSubview:self.tab];
        [NSLayoutConstraint activateConstraints:@[
            [self.tab.centerXAnchor constraintEqualToAnchor:w.centerXAnchor],
            [self.tab.topAnchor constraintEqualToAnchor:w.safeAreaLayoutGuide.topAnchor constant:2],
            [self.tab.widthAnchor constraintLessThanOrEqualToAnchor:w.widthAnchor constant:-120],
        ]];
    }
    [w bringSubviewToFront:self.tab];
}
- (void)open {
    [self.tab removeFromSuperview];
    [[BFOilLibrary shared] show];
}
@end

static BFOilTab *sTab;

void BFOilUIStart(void) {
    if (sTab) return;
    ApplyActive();                                 // the pattern you had on last time
    sTab = [BFOilTab new];
    sTab.timer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:sTab selector:@selector(tick) userInfo:nil repeats:YES];
}

void BFOilShowLibrary(void) { [[BFOilLibrary shared] show]; }

#pragma mark - oil color picker

// What oil of this hue looks like on the lane: the shader blends wood with the color (about 40% for
// normal oil) and only uses the hue.
static UIColor *OilOnWood(CGFloat hue) {
    CGFloat r, g, b;
    [[UIColor colorWithHue:hue saturation:1 brightness:1 alpha:1] getRed:&r green:&g blue:&b alpha:NULL];
    CGFloat wood[3] = { 0.86, 0.70, 0.50 }, c[3] = { r, g, b };
    for (int i = 0; i < 3; i++) c[i] = wood[i] * (1 + 0.45 * (c[i] - 1));
    return [UIColor colorWithRed:c[0] green:c[1] blue:c[2] alpha:1];
}

static UIImage *RainbowTrack(void) {
    CGSize sz = CGSizeMake(240, 8);
    return [[[UIGraphicsImageRenderer alloc] initWithSize:sz] imageWithActions:^(UIGraphicsImageRendererContext *ctx) {
        for (int x = 0; x < (int)sz.width; x++) {
            [[UIColor colorWithHue:x / sz.width saturation:1 brightness:1 alpha:1] setFill];
            UIRectFill(CGRectMake(x, 0, 1, sz.height));
        }
    }];
}

void BFOilShowColorPicker(void) {
    UIView *overlay, *card;
    UIStackView *stack = OSheet(&overlay, &card, NO);
    if (!overlay) return;
    __weak UIView *weakOverlay = overlay;
    [stack addArrangedSubview:OLabel(@"Oil color", 20, UIFontWeightHeavy, UIColor.whiteColor)];
    [stack addArrangedSubview:OLabel(@"The color the lane shows oil in. Thicker oil is a bit darker, like in the game.", 12, UIFontWeightRegular, ODim(0.55))];

    UILabel *preview = OLabel(@"", 13, UIFontWeightBold, [UIColor colorWithWhite:0.15 alpha:1]);
    preview.textAlignment = NSTextAlignmentCenter;
    preview.layer.cornerRadius = 12;
    preview.clipsToBounds = YES;
    [preview.heightAnchor constraintEqualToConstant:54].active = YES;
    [stack addArrangedSubview:preview];

    UISlider *slider = [UISlider new];
    UIImage *track = [RainbowTrack() resizableImageWithCapInsets:UIEdgeInsetsZero resizingMode:UIImageResizingModeStretch];
    [slider setMinimumTrackImage:track forState:UIControlStateNormal];
    [slider setMaximumTrackImage:track forState:UIControlStateNormal];
    slider.minimumValue = 0;
    slider.maximumValue = 0.999f;
    [stack addArrangedSubview:slider];

    void (^show)(void) = ^{
        BOOL game = gBF.oilHue < 0;
        preview.backgroundColor = game ? [UIColor colorWithRed:0.86 green:0.70 blue:0.50 alpha:1] : OilOnWood(gBF.oilHue);
        preview.text = game ? @"The game's own color" : @"Oil on the lane";
        if (!game) slider.value = gBF.oilHue;
    };
    void (^pick)(CGFloat) = ^(CGFloat hue) {
        gBF.oilHue = hue;
        BFSaveConfig();
        BFOilApplyHue();
        show();
    };
    [slider addAction:[UIAction actionWithHandler:^(UIAction *a) { pick(slider.value); }] forControlEvents:UIControlEventValueChanged];

    NSMutableArray *dots = [NSMutableArray array];
    for (NSNumber *h in @[ @0.0, @0.08, @0.13, @0.33, @0.47, @0.58, @0.75, @0.9 ]) {
        UIButton *b = [UIButton buttonWithType:UIButtonTypeCustom];
        b.backgroundColor = [UIColor colorWithHue:h.doubleValue saturation:0.85 brightness:0.95 alpha:1];
        b.layer.cornerRadius = 15;
        b.layer.borderWidth = 2;
        b.layer.borderColor = ODim(0.25).CGColor;
        [b.widthAnchor constraintEqualToConstant:30].active = YES;
        [b.heightAnchor constraintEqualToConstant:30].active = YES;
        CGFloat hue = h.doubleValue;
        [b addAction:[UIAction actionWithHandler:^(UIAction *a) { pick(hue); }] forControlEvents:UIControlEventTouchUpInside];
        [dots addObject:b];
    }
    UIStackView *row = OStack(dots, UILayoutConstraintAxisHorizontal, 8);
    row.distribution = UIStackViewDistributionEqualSpacing;
    [stack addArrangedSubview:row];

    UIButton *reset = OButton(@"Use the game's color", NO, nil, nil);
    [reset addAction:[UIAction actionWithHandler:^(UIAction *a) {
        gBF.oilHue = -1;
        BFSaveConfig();
        show();
    }] forControlEvents:UIControlEventTouchUpInside];
    UIButton *done = OButton(@"Done", YES, nil, nil);
    [done addAction:[UIAction actionWithHandler:^(UIAction *a) { ODismiss(weakOverlay); }] forControlEvents:UIControlEventTouchUpInside];
    UIStackView *btns = OStack(@[reset, done], UILayoutConstraintAxisHorizontal, 8);
    btns.distribution = UIStackViewDistributionFillEqually;
    [stack addArrangedSubview:btns];
    if (gBF.oilHue < 0) slider.value = 0.4f;
    show();
    OPresent(overlay, card, NO);
}

#pragma mark - Kegel pattern files (.zip from the Kegel Pattern Library, .Pattern, .txt)

// .Pattern: Kegel's JSON (Name, Distance, ReverseDropBrushDistance, Forward/ReverseLoadscreens with
// Start, Stop, Loads, SpeedIps, EndDistance, Microliter). Exact distances.
static NSDictionary *KegelFromJSON(NSData *data) {
    if (data.length >= 3 && ((const uint8_t *)data.bytes)[0] == 0xEF) data = [data subdataWithRange:NSMakeRange(3, data.length - 3)];
    NSDictionary *j = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![j isKindOfClass:[NSDictionary class]] || ![j[@"ForwardLoadscreens"] isKindOfClass:[NSArray class]]) return nil;
    NSMutableArray *fr[2] = { [NSMutableArray array], [NSMutableArray array] };
    int ul = 50;
    NSArray *src[2] = { j[@"ForwardLoadscreens"], j[@"ReverseLoadscreens"] };
    for (int d = 0; d < 2; d++) {
        if (![src[d] isKindOfClass:[NSArray class]]) continue;
        for (NSDictionary *s in src[d]) {
            if (![s isKindOfClass:[NSDictionary class]] || [s[@"SpeedIps"] intValue] <= 0 || [s[@"Stop"] intValue] <= 0) continue;
            if ([s[@"Microliter"] intValue] > 0) ul = [s[@"Microliter"] intValue];
            [fr[d] addObject:@[ @([s[@"Start"] intValue]), @([s[@"Stop"] intValue]), @([s[@"Loads"] intValue]),
                                @([s[@"SpeedIps"] intValue]), @([s[@"EndDistance"] floatValue]) ]];
        }
    }
    if (!fr[0].count) return nil;
    NSString *name = [j[@"Name"] isKindOfClass:[NSString class]] ? j[@"Name"] : @"Kegel pattern";
    return @{ @"name": name, @"feet": @([j[@"Distance"] intValue]), @"drop": @([j[@"ReverseDropBrushDistance"] intValue]),
              @"ul": @(ul), @"fwd": fr[0], @"rev": fr[1] };
}

// .txt: the Kegel text export, the same format as the game's 48 built-in patterns. Fixed line positions
// (checked against the game's Route 66 and the 2026 PBA Regional 37 file): name 1, uL 10, distance 12,
// reverse brush drop 13; 15-line columns: forward start 14, stop 29, loads 44, speed 59; reverse start 75,
// stop 90, loads 105, speed 120; forward end distances 136, reverse end distances 211.
static NSDictionary *KegelFromText(NSString *text) {
    NSArray *L = [[text stringByReplacingOccurrencesOfString:@"\r" withString:@""] componentsSeparatedByString:@"\n"];
    if (L.count < 226 || ![[L[0] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] isEqualToString:@"-1"]) return nil;
    NSString *(^at)(NSUInteger) = ^NSString *(NSUInteger i) {
        return i < L.count ? [L[i] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] : @"";
    };
    NSMutableArray *fr[2] = { [NSMutableArray array], [NSMutableArray array] };
    NSUInteger base[2][5] = { { 14, 29, 44, 59, 136 }, { 75, 90, 105, 120, 211 } };
    for (int d = 0; d < 2; d++) {
        for (NSUInteger i = 0; i < 15; i++) {
            NSString *st = at(base[d][0] + i), *sp = at(base[d][3] + i);
            if (!st.length || !sp.length) break;
            [fr[d] addObject:@[ @(st.intValue), @(at(base[d][1] + i).intValue), @(at(base[d][2] + i).intValue),
                                @(sp.intValue), @(at(base[d][4] + i).floatValue) ]];
        }
    }
    if (!fr[0].count) return nil;
    return @{ @"name": at(1).length ? at(1) : @"Kegel pattern", @"feet": @(at(12).intValue), @"drop": @(at(13).intValue),
              @"ul": @(at(10).intValue ?: 50), @"fwd": fr[0], @"rev": fr[1] };
}

// PDF from the Kegel Pattern Library (website / app "download"): the data sheet. Its text has one line
// per step, "# START STOP LOADS MICS SPEED BUFF TANK from -> to T.OIL" (e.g. "2 2L 6R 1 50 14 500 A - 4 →6
// 1,650"), forward rows before "REVERSE LOADS DATA" and reverse rows after it; "37 FEET" (distance) then
// "30 FEET" (drop brush). Distances are whole feet, which is what the engine works in (whole-foot rows).
static int KegelBoard(NSString *t) {               // "2L" -> 2, "6R" -> 34, "20" -> 20
    t = [[t stringByReplacingOccurrencesOfString:@" " withString:@""] uppercaseString];
    int n = t.intValue;
    if ([t hasSuffix:@"R"]) return 40 - n;
    return n;
}

static NSDictionary *KegelFromPDFText(NSString *text, NSString *title) {
    if (!text.length) return nil;
    static NSRegularExpression *row, *feet;
    if (!row) {
        row = [NSRegularExpression regularExpressionWithPattern:
               @"(?<![\\d.,])(\\d{1,2})\\s+(\\d{1,2}\\s?[LR]|20)\\s+(\\d{1,2}\\s?[LR]|20)\\s+(\\d{1,2})\\s+(\\d{1,3})\\s+(\\d{1,2})\\s+(\\d{1,4})"
               @"\\s+(?:[A-Za-z][^\\d]{0,24}?)?\\s*(\\d{1,2}(?:\\.\\d+)?)\\s*(?:\\u2192|->|>|\\u2013|-|to)?\\s*(\\d{1,2}(?:\\.\\d+)?)\\s+[\\d,]+"
                                                       options:NSRegularExpressionCaseInsensitive error:nil];
        feet = [NSRegularExpression regularExpressionWithPattern:@"(\\d{1,2})\\s*FEET" options:NSRegularExpressionCaseInsensitive error:nil];
    }
    NSUInteger revAt = [text rangeOfString:@"REVERSE\\s+LOADS" options:NSCaseInsensitiveSearch | NSRegularExpressionSearch].location;
    NSMutableArray *fr[2] = { [NSMutableArray array], [NSMutableArray array] };
    int ul = 0, lastNum[2] = { 0, 0 };
    for (NSTextCheckingResult *m in [row matchesInString:text options:0 range:NSMakeRange(0, text.length)]) {
        NSString *(^g)(NSUInteger) = ^NSString *(NSUInteger i) { return [text substringWithRange:[m rangeAtIndex:i]]; };
        int num = g(1).intValue;
        int d = (revAt != NSNotFound) ? (m.range.location > revAt ? 1 : 0)
                                      : ((lastNum[1] > 0 || (num == 1 && lastNum[0] > 0)) ? 1 : 0);   // no heading: numbering restarts
        if (num != lastNum[d] + 1) continue;       // rows come numbered 1, 2, 3... in each table
        lastNum[d] = num;
        if (!ul) ul = g(5).intValue;
        [fr[d] addObject:@[ @(KegelBoard(g(2))), @(KegelBoard(g(3))), @(g(4).intValue), @(g(6).intValue), @(g(9).floatValue) ]];
    }
    if (!fr[0].count) return nil;
    NSArray *ft = [feet matchesInString:text options:0 range:NSMakeRange(0, text.length)];
    int distance = ft.count > 0 ? [text substringWithRange:[ft[0] rangeAtIndex:1]].intValue : 0;
    int drop = ft.count > 1 ? [text substringWithRange:[ft[1] rangeAtIndex:1]].intValue : 0;
    NSRange dl = [text rangeOfString:@"DROP\\s*BRUSH:" options:NSCaseInsensitiveSearch | NSRegularExpressionSearch];
    if (dl.location != NSNotFound) {                // "DROP BRUSH: 30 FEET" when the label sits next to its value
        NSTextCheckingResult *m = [feet firstMatchInString:text options:0 range:NSMakeRange(NSMaxRange(dl), MIN((NSUInteger)24, text.length - NSMaxRange(dl)))];
        if (m) drop = [text substringWithRange:[m rangeAtIndex:1]].intValue;
    }
    return @{ @"name": title.length ? title : @"Kegel pattern", @"feet": @(distance), @"drop": @(drop),
              @"ul": @(ul ?: 50), @"fwd": fr[0], @"rev": fr[1] };
}

static NSDictionary *KegelFromPDF(NSData *data, NSString *fileName) {
    PDFDocument *doc = [[PDFDocument alloc] initWithData:data];
    if (!doc) return nil;
    NSMutableString *text = [NSMutableString string];
    for (NSInteger i = 0; i < doc.pageCount; i++) { NSString *t = [doc pageAtIndex:i].string; if (t) [text appendFormat:@"%@\n", t]; }
    NSString *title = doc.documentAttributes[PDFDocumentTitleAttribute];
    if (![title isKindOfClass:[NSString class]] || !title.length)
        title = [[fileName stringByDeletingPathExtension] stringByReplacingOccurrencesOfString:@"_" withString:@" "];
    return KegelFromPDFText(text, title);
}

// The first file in a .zip whose name ends with ext (skips macOS "__MACOSX" copies). Stored or deflated.
static NSData *ZipFile(NSData *zip, NSString *ext) {
    const uint8_t *b = (const uint8_t *)zip.bytes;
    NSUInteger n = zip.length;
    auto u16 = [&](NSUInteger o) -> uint32_t { return o + 2 <= n ? (uint32_t)(b[o] | b[o + 1] << 8) : 0; };
    auto u32 = [&](NSUInteger o) -> uint32_t { return o + 4 <= n ? (uint32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24) : 0; };
    if (n < 22) return nil;
    NSUInteger eocd = NSNotFound;
    for (NSUInteger o = n - 22; o + 1 > 0 && n - o < 70000; o--) { if (u32(o) == 0x06054b50) { eocd = o; break; } if (o == 0) break; }
    if (eocd == NSNotFound) return nil;
    NSUInteger count = u16(eocd + 10), cd = u32(eocd + 16);
    for (NSUInteger i = 0; i < count && cd + 46 <= n; i++) {
        if (u32(cd) != 0x02014b50) break;
        uint32_t method = u16(cd + 10), csize = u32(cd + 20), usize = u32(cd + 24);
        uint32_t nl = u16(cd + 28), xl = u16(cd + 30), cl = u16(cd + 32), lho = u32(cd + 42);
        NSString *name = cd + 46 + nl <= n ? [[NSString alloc] initWithBytes:b + cd + 46 length:nl encoding:NSUTF8StringEncoding] : nil;
        cd += 46 + nl + xl + cl;
        if (!name || [name hasPrefix:@"__MACOSX"] || ![name.lowercaseString hasSuffix:ext]) continue;
        if (lho + 30 > n || u32(lho) != 0x04034b50) continue;
        NSUInteger data = lho + 30 + u16(lho + 26) + u16(lho + 28);
        if (data + csize > n) continue;
        NSData *raw = [zip subdataWithRange:NSMakeRange(data, csize)];
        if (method == 0) return raw;
        if (method == 8) {                         // raw deflate (Apple's "zlib" algorithm)
            NSData *out = [raw decompressedDataUsingAlgorithm:NSDataCompressionAlgorithmZlib error:nil];
            if (out && (!usize || out.length == usize)) return out;
        }
    }
    return nil;
}

static NSDictionary *KegelImport(NSData *data, NSString *fileName) {
    if (!data.length) return nil;
    const uint8_t *b = (const uint8_t *)data.bytes;
    NSDictionary *k = nil;
    if (data.length > 4 && b[0] == 'P' && b[1] == 'K') {
        NSData *p = ZipFile(data, @".pattern");
        if (p) k = KegelFromJSON(p);
        if (!k) { NSData *t = ZipFile(data, @".txt"); if (t) k = KegelFromText([[NSString alloc] initWithData:t encoding:NSUTF8StringEncoding] ?: [[NSString alloc] initWithData:t encoding:NSISOLatin1StringEncoding]); }
        if (!k) { NSData *pdf = ZipFile(data, @".pdf"); if (pdf) k = KegelFromPDF(pdf, fileName); }
    } else if (data.length > 4 && b[0] == '%' && b[1] == 'P' && b[2] == 'D' && b[3] == 'F') {
        k = KegelFromPDF(data, fileName);
    } else {
        k = KegelFromJSON(data);
        if (!k) k = KegelFromText([[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: [[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding]);
    }
    if (!k) return nil;
    NSMutableDictionary *p = [k mutableCopy];
    p[@"id"] = NSUUID.UUID.UUIDString;
    p[@"fwd"] = CleanSteps(k[@"fwd"]);
    p[@"rev"] = CleanSteps(k[@"rev"]);
    p[@"base"] = @0;
    p[@"exact"] = @YES;                            // the file's own step distances
    NSString *name = [k[@"name"] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    p[@"name"] = name.length ? [name substringToIndex:MIN((NSUInteger)40, name.length)] : (fileName.stringByDeletingPathExtension ?: @"Kegel pattern");
    return p;
}

@interface BFKegelPicker : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, copy) void (^done)(NSDictionary *pattern, NSString *error);
@end
@implementation BFKegelPicker
- (void)documentPicker:(UIDocumentPickerViewController *)c didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *u = urls.firstObject;
    BOOL scoped = [u startAccessingSecurityScopedResource];
    NSData *d = u ? [NSData dataWithContentsOfURL:u] : nil;
    if (scoped) [u stopAccessingSecurityScopedResource];
    NSDictionary *p = KegelImport(d, u.lastPathComponent);
    ODonePresenting();
    if (self.done) self.done(p, p ? nil : @"That file isn't a Kegel pattern (a .pdf, .zip, .Pattern or .txt from the Kegel Pattern Library)");
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)c { ODonePresenting(); }
@end

static BFKegelPicker *sKegelPicker;

static void ImportKegelFile(void (^done)(NSDictionary *pattern, NSString *error)) {
    UIDocumentPickerViewController *pick = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeItem ] asCopy:YES];
    sKegelPicker = [BFKegelPicker new];
    sKegelPicker.done = done;
    pick.delegate = sKegelPicker;
    pick.allowsMultipleSelection = NO;
    OPresentVC(pick);
}
