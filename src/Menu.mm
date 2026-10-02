#import <UIKit/UIKit.h>
#import "BFShared.h"
#import "Logo.h"
#include <math.h>

// The menu is plain UIKit drawn on top of the game. Nothing is on screen until you shake.

static UIColor *Accent(void) { return [UIColor colorWithRed:1.0 green:0.48 blue:0.10 alpha:1.0]; }
static UIColor *Dim(CGFloat a) { return [UIColor colorWithWhite:1.0 alpha:a]; }

NSString *BFPinsText(uint16_t mask) {             // 0x240 -> "7-10"
    mask &= BF_ALL_PINS;
    if (mask == BF_ALL_PINS) return @"a full rack";
    NSMutableArray<NSString *> *pins = [NSMutableArray array];
    for (int i = 0; i < 10; i++) if (mask & (1u << i)) [pins addObject:[NSString stringWithFormat:@"%d", i + 1]];
    return pins.count ? [pins componentsJoinedByString:@"-"] : @"no pins";
}

static UIWindow *HostWindow(void) {
    UIWindow *fallback = nil;
    for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) continue;
        for (UIWindow *w in ((UIWindowScene *)scene).windows) {
            if (w.isKeyWindow) return w;
            if (!fallback && !w.hidden) fallback = w;
        }
    }
    if (!fallback) fallback = [UIApplication sharedApplication].windows.firstObject;
    return fallback;
}

@interface BFMenu : NSObject <UITextFieldDelegate>
@property (nonatomic, strong) UIView *menuOverlay;
@property (nonatomic, strong) UIView *card;
@property (nonatomic, strong) NSLayoutConstraint *cardCenterY;
@property (nonatomic, strong) UILabel *statusLabel, *ballLabel, *speedLabel, *arsenalLabel;
@property (nonatomic, strong) UISwitch *skinSwitch, *pinSwitch, *spareSwitch, *autoSwitch;
@property (nonatomic, strong) UILabel *autoLabel, *fpsLabel, *oilLabel;
@property (nonatomic, strong) UISwitch *oilMirrorSwitch, *oilBreakSwitch, *oilInvisSwitch, *oilThickSwitch;
@property (nonatomic, strong) UIButton *oilColorButton;
@property (nonatomic, strong) UISwitch *specSwitch, *fpsSwitch, *privacySwitch, *unstickSwitch, *ipv4Switch;
@property (nonatomic, strong) UIButton *autoButton, *skipButton, *debugButton, *logButton, *netButton;
@property (nonatomic, strong) UISlider *speedSlider;
@property (nonatomic, strong) UITextField *searchField;
@property (nonatomic, strong) NSTimer *refreshTimer;
@property (nonatomic) BOOL menuShowing;
@property (nonatomic, strong) UIView *pickerOverlay;
@property (nonatomic, strong) NSMutableArray<UIButton *> *pinButtons;
@property (nonatomic, strong) UIButton *rackButton;
@property (nonatomic, strong) UIButton *resumeButton;
@property (nonatomic) uint16_t pickerMask;
+ (instancetype)shared;
- (BOOL)menuVisible;
- (void)showMenu;
- (void)hideMenu;
- (void)showPickerWithMask:(uint16_t)mask;
- (void)hidePicker;
- (BOOL)pickerVisible;
@end

@implementation BFMenu

+ (instancetype)shared {
    static BFMenu *m;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ m = [BFMenu new]; });
    return m;
}

- (instancetype)init {
    if ((self = [super init])) {
        NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
        [nc addObserver:self selector:@selector(keyboardChanged:) name:UIKeyboardWillChangeFrameNotification object:nil];
        [nc addObserver:self selector:@selector(keyboardHidden:) name:UIKeyboardWillHideNotification object:nil];
    }
    return self;
}

#pragma mark - little UI helpers

- (UILabel *)label:(NSString *)text size:(CGFloat)size weight:(UIFontWeight)weight color:(UIColor *)color {
    UILabel *l = [UILabel new];
    l.text = text;
    l.font = [UIFont systemFontOfSize:size weight:weight];
    l.textColor = color;
    l.numberOfLines = 0;
    return l;
}

- (UIView *)sectionTitle:(NSString *)text {
    return [self label:text.uppercaseString size:12 weight:UIFontWeightBold color:Dim(0.45)];
}

- (UIView *)rowWithTitle:(NSString *)title detail:(NSString *)detail control:(UIView *)control {
    UIStackView *texts = [[UIStackView alloc] initWithArrangedSubviews:@[[self label:title size:16 weight:UIFontWeightSemibold color:UIColor.whiteColor]]];
    texts.axis = UILayoutConstraintAxisVertical;
    texts.spacing = 2;
    if (detail.length) [texts addArrangedSubview:[self label:detail size:12 weight:UIFontWeightRegular color:Dim(0.55)]];
    UIStackView *row = [[UIStackView alloc] initWithArrangedSubviews:@[texts, control]];
    row.axis = UILayoutConstraintAxisHorizontal;
    row.alignment = UIStackViewAlignmentCenter;
    row.spacing = 12;
    [control setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [control setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    return row;
}

- (UISwitch *)switchOn:(BOOL)on action:(SEL)action {
    UISwitch *s = [UISwitch new];
    s.on = on;
    s.onTintColor = Accent();
    [s addTarget:self action:action forControlEvents:UIControlEventValueChanged];
    return s;
}

- (UIButton *)button:(NSString *)title filled:(BOOL)filled small:(BOOL)small action:(SEL)action {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    [b setTitle:title forState:UIControlStateNormal];
    [b setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    b.titleLabel.font = [UIFont systemFontOfSize:small ? 13 : 15 weight:UIFontWeightSemibold];
    b.titleLabel.adjustsFontSizeToFitWidth = YES;
    b.backgroundColor = filled ? Accent() : Dim(0.12);
    b.layer.cornerRadius = 10;
    b.contentEdgeInsets = small ? UIEdgeInsetsMake(8, 6, 8, 6) : UIEdgeInsetsMake(10, 14, 10, 14);
    [b addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return b;
}

// logo  BowlingPlus (GitHub)  ·  Donate (GitHub Sponsors), on one line
- (UIView *)footer {
    UIImageView *logo = [[UIImageView alloc] initWithImage:[UIImage imageWithData:[NSData dataWithBytes:kBPLogoPNG length:kBPLogoPNGLen] scale:3]];
    logo.contentMode = UIViewContentModeScaleAspectFit;
    [logo.widthAnchor constraintEqualToConstant:25].active = YES;
    [logo.heightAnchor constraintEqualToConstant:25].active = YES;
    UIButton *gh = [self linkButton:@"BowlingPlus" url:@"https://github.com/NicholasBly/BowlingPlus"];
    UIButton *donate = [self linkButton:@"\u2665 Donate" url:@"https://github.com/sponsors/NicholasBly"];
    UILabel *dot = [self label:@"\u00B7" size:15 weight:UIFontWeightBold color:Dim(0.35)];
    UIStackView *row = [self hstack:@[logo, gh, dot, donate] spacing:8];
    row.alignment = UIStackViewAlignmentCenter;
    UIView *wrap = [UIView new];                  // centered in the menu
    row.translatesAutoresizingMaskIntoConstraints = NO;
    [wrap addSubview:row];
    [NSLayoutConstraint activateConstraints:@[
        [row.topAnchor constraintEqualToAnchor:wrap.topAnchor constant:6],
        [row.bottomAnchor constraintEqualToAnchor:wrap.bottomAnchor],
        [row.centerXAnchor constraintEqualToAnchor:wrap.centerXAnchor],
        [row.leadingAnchor constraintGreaterThanOrEqualToAnchor:wrap.leadingAnchor],
    ]];
    return wrap;
}

- (UIButton *)linkButton:(NSString *)title url:(NSString *)url {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    NSDictionary *attrs = @{ NSFontAttributeName: [UIFont systemFontOfSize:15 weight:UIFontWeightBold],
                             NSForegroundColorAttributeName: Accent(),
                             NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle) };
    [b setAttributedTitle:[[NSAttributedString alloc] initWithString:title attributes:attrs] forState:UIControlStateNormal];
    [b addAction:[UIAction actionWithHandler:^(UIAction *a) {
        [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url] options:@{} completionHandler:nil];
    }] forControlEvents:UIControlEventTouchUpInside];
    return b;
}

- (UIStackView *)hstack:(NSArray<UIView *> *)views spacing:(CGFloat)spacing {
    UIStackView *s = [[UIStackView alloc] initWithArrangedSubviews:views];
    s.axis = UILayoutConstraintAxisHorizontal;
    s.spacing = spacing;
    s.alignment = UIStackViewAlignmentCenter;
    return s;
}

- (UIView *)cardView {
    UIView *card = [UIView new];
    card.translatesAutoresizingMaskIntoConstraints = NO;
    card.backgroundColor = [UIColor colorWithRed:0.08 green:0.09 blue:0.11 alpha:0.97];
    card.layer.cornerRadius = 20;
    card.layer.borderWidth = 1;
    card.layer.borderColor = Dim(0.08).CGColor;
    card.clipsToBounds = YES;
    return card;
}

- (UIStackView *)vstack {
    UIStackView *s = [UIStackView new];
    s.axis = UILayoutConstraintAxisVertical;
    s.spacing = 14;
    s.translatesAutoresizingMaskIntoConstraints = NO;
    s.layoutMargins = UIEdgeInsetsMake(18, 18, 18, 18);
    s.layoutMarginsRelativeArrangement = YES;
    return s;
}

#pragma mark - main menu

- (void)buildMenuIn:(UIView *)host {
    UIView *overlay = [[UIView alloc] initWithFrame:host.bounds];
    overlay.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    overlay.backgroundColor = [UIColor colorWithWhite:0 alpha:0.45];
    UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(overlayTapped:)];
    tap.cancelsTouchesInView = NO;
    [overlay addGestureRecognizer:tap];

    UIView *card = [self cardView];
    [overlay addSubview:card];
    self.card = card;

    UIScrollView *scroll = [UIScrollView new];
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    scroll.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
    [card addSubview:scroll];
    UIStackView *stack = [self vstack];
    [scroll addSubview:stack];

    // header
    UIButton *close = [UIButton buttonWithType:UIButtonTypeSystem];
    [close setTitle:@"\u2715" forState:UIControlStateNormal];
    close.titleLabel.font = [UIFont systemFontOfSize:20 weight:UIFontWeightBold];
    [close setTitleColor:Dim(0.7) forState:UIControlStateNormal];
    [close addTarget:self action:@selector(hideMenu) forControlEvents:UIControlEventTouchUpInside];
    [stack addArrangedSubview:[self hstack:@[[self label:@"\U0001F3B3 BowlingPlus" size:22 weight:UIFontWeightHeavy color:UIColor.whiteColor], close] spacing:8]];
    self.statusLabel = [self label:@"" size:13 weight:UIFontWeightMedium color:Accent()];
    [stack addArrangedSubview:self.statusLabel];
    self.resumeButton = [self button:@"Turn BowlingPlus back on" filled:YES small:NO action:@selector(resumeTapped)];
    [stack addArrangedSubview:self.resumeButton];

    // arsenal search (near the top so the keyboard never covers it)
    [stack addArrangedSubview:[self sectionTitle:@"Arsenal search"]];
    UITextField *f = [UITextField new];
    f.attributedPlaceholder = [[NSAttributedString alloc] initWithString:@"Ball name, e.g. match up"
                                                              attributes:@{ NSForegroundColorAttributeName: Dim(0.35) }];
    f.textColor = UIColor.whiteColor;
    f.backgroundColor = Dim(0.08);
    f.layer.cornerRadius = 10;
    f.leftView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 10, 10)];
    f.leftViewMode = UITextFieldViewModeAlways;
    f.clearButtonMode = UITextFieldViewModeWhileEditing;
    f.returnKeyType = UIReturnKeySearch;
    f.autocorrectionType = UITextAutocorrectionTypeNo;
    f.autocapitalizationType = UITextAutocapitalizationTypeNone;
    f.keyboardAppearance = UIKeyboardAppearanceDark;
    f.delegate = self;
    [f.heightAnchor constraintEqualToConstant:40].active = YES;
    [f setContentHuggingPriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    self.searchField = f;
    UIButton *go = [self button:@"Search" filled:YES small:NO action:@selector(searchTapped)];
    UIButton *clear = [self button:@"Clear" filled:NO small:NO action:@selector(clearTapped)];
    [go setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [clear setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [stack addArrangedSubview:[self hstack:@[f, go, clear] spacing:8]];
    self.arsenalLabel = [self label:@"" size:12 weight:UIFontWeightRegular color:Dim(0.55)];
    [stack addArrangedSubview:self.arsenalLabel];

    // fixes
    [stack addArrangedSubview:[self sectionTitle:@"Fixes"]];
    self.skinSwitch = [self switchOn:gBF.textureFix action:@selector(skinChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Match Up skins"
                                          detail:@"Fixes the Match Up Pearl/BP ball textures."
                                         control:self.skinSwitch]];
    self.ballLabel = [self label:@"" size:12 weight:UIFontWeightRegular color:Dim(0.5)];
    [stack addArrangedSubview:self.ballLabel];
    self.pinSwitch = [self switchOn:gBF.pinFix action:@selector(pinChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Pin physics fix"
                                          detail:@"Fast pins can't fly through other pins, and pins clipped low at the base can tip over properly. Practice only."
                                         control:self.pinSwitch]];
    self.specSwitch = [self switchOn:gBF.pinSpec action:@selector(specChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Improve spinning pin collision (experimental)"
                                          detail:@"Uses Unity's speculative collisions, which also predict spin."
                                         control:self.specSwitch]];
    self.unstickSwitch = [self switchOn:gBF.unstick action:@selector(unstickChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Fix connection"
                                          detail:@"If loading sits on \"connecting\" for 30 s, shows the game's gray offline button. A loading circle stuck for 30 s gets hidden so you can try again."
                                         control:self.unstickSwitch]];
    self.ipv4Switch = [self switchOn:gBF.gameIPv4 action:@selector(ipv4Changed:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Game server over IPv4"
                                          detail:@"The game always picks IPv6 when some DNS servers offer it, but its servers don't answer on IPv6, so it hangs indefinitely. This forces IPv4."
                                         control:self.ipv4Switch]];

    [stack addArrangedSubview:[self sectionTitle:@"Display & startup"]];
    self.fpsSwitch = [self switchOn:gBF.fps120 action:@selector(fpsChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"120 FPS mode"
                                          detail:@"Runs menus and gameplay at 120 FPS on 120 Hz screens: smoother, with faster touch response. The game normally uses 30 FPS menus / 60 FPS play. Uses more battery."
                                         control:self.fpsSwitch]];
    self.fpsLabel = [self label:@"" size:12 weight:UIFontWeightRegular color:Dim(0.5)];
    [stack addArrangedSubview:self.fpsLabel];
    self.privacySwitch = [self switchOn:gBF.autoPrivacy action:@selector(privacyChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Auto-accept the privacy popup"
                                          detail:@"Presses \"Sign up\" for you on the My.Games privacy page that shows every launch."
                                         control:self.privacySwitch]];

    // fun
    [stack addArrangedSubview:[self sectionTitle:@"Practice fun (offline only)"]];
    self.speedLabel = [self label:@"" size:16 weight:UIFontWeightBold color:Accent()];
    [self.speedLabel setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    UILabel *speedTitle = [self label:@"Ball speed" size:16 weight:UIFontWeightSemibold color:UIColor.whiteColor];
    UIButton *reset = [self button:@"Reset to 1x" filled:NO small:YES action:@selector(speedReset)];
    [reset setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [stack addArrangedSubview:[self hstack:@[speedTitle, self.speedLabel, reset] spacing:10]];
    UISlider *slider = [UISlider new];
    slider.minimumValue = 1;
    slider.maximumValue = BF_MAX_SPEED;
    slider.minimumTrackTintColor = Accent();
    [slider addTarget:self action:@selector(speedChanged:) forControlEvents:UIControlEventValueChanged];
    [slider addTarget:self action:@selector(speedDone:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside];
    self.speedSlider = slider;
    [stack addArrangedSubview:slider];
    self.spareSwitch = [self switchOn:gBF.spareMode action:@selector(spareChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Spare shooting mode"
                                          detail:@"Pick which pins stand at the start of every frame."
                                         control:self.spareSwitch]];
    self.autoSwitch = [self switchOn:gBF.spareAuto action:@selector(autoChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Auto-rack"
                                          detail:@"Sets up the same pins every frame without asking. Tip: pick your pins once and tap Auto in the pin picker."
                                         control:self.autoSwitch]];
    self.autoLabel = [self label:@"" size:12 weight:UIFontWeightSemibold color:Accent()];
    [stack addArrangedSubview:self.autoLabel];

    [stack addArrangedSubview:[self sectionTitle:@"Oil (practice)"]];
    self.oilMirrorSwitch = [self switchOn:gBF.oilMirrorFix action:@selector(oilMirrorChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Fix oil display side"
                                          detail:@"The game drew the oil mirrored, so breakdown and carrydown showed up on the wrong side. Now they show where your ball went."
                                         control:self.oilMirrorSwitch]];
    self.oilColorButton = [UIButton buttonWithType:UIButtonTypeCustom];
    self.oilColorButton.layer.cornerRadius = 15;
    self.oilColorButton.layer.borderWidth = 2;
    self.oilColorButton.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.35].CGColor;
    [self.oilColorButton.widthAnchor constraintEqualToConstant:44].active = YES;
    [self.oilColorButton.heightAnchor constraintEqualToConstant:30].active = YES;
    [self.oilColorButton addTarget:self action:@selector(oilColorTapped) forControlEvents:UIControlEventTouchUpInside];
    [stack addArrangedSubview:[self rowWithTitle:@"Oil color"
                                          detail:@"Pick the color the lane shows oil in, or keep the game's."
                                         control:self.oilColorButton]];
    self.oilThickSwitch = [self switchOn:gBF.oilThickness action:@selector(oilThickChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Show oil thickness"
                                          detail:@"Stronger shading by oil thickness (darker = more oil) instead of the game's look. Looks only, the ball feels the same."
                                         control:self.oilThickSwitch]];
    self.oilBreakSwitch = [self switchOn:gBF.oilBreakdown action:@selector(oilBreakChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Show oil breakdown"
                                          detail:@"Redraws the lane oil after every shot so you can watch it break down over the game."
                                         control:self.oilBreakSwitch]];
    self.oilInvisSwitch = [self switchOn:gBF.oilInvisible action:@selector(oilInvisChanged:)];
    [stack addArrangedSubview:[self rowWithTitle:@"Invisible oil"
                                          detail:@"Hides the oil and plays a random unlocked game pattern each game. Read the lane like the real thing."
                                         control:self.oilInvisSwitch]];
    [stack addArrangedSubview:[self button:@"\U0001F6E2 Custom oil patterns" filled:NO small:NO action:@selector(oilLibraryTapped)]];
    self.oilLabel = [self label:@"" size:12 weight:UIFontWeightSemibold color:Accent()];
    [stack addArrangedSubview:self.oilLabel];

    [stack addArrangedSubview:[self sectionTitle:@"Help"]];
    self.debugButton = [self button:@"Copy debug info" filled:NO small:NO action:@selector(copyDebugTapped)];
    [stack addArrangedSubview:self.debugButton];
    self.logButton = [self button:@"Copy log" filled:NO small:NO action:@selector(copyLogTapped)];
    self.netButton = [self button:@"Run connection test" filled:NO small:NO action:@selector(netTestTapped)];
    UIStackView *logRow = [self hstack:@[self.logButton, self.netButton] spacing:8];
    logRow.distribution = UIStackViewDistributionFillEqually;
    [stack addArrangedSubview:logRow];
    [stack addArrangedSubview:[self label:@"The log records loading, the connection and network checks from the moment the game starts. Run the connection test, wait ~20 s, then Copy log."
                                     size:11 weight:UIFontWeightRegular color:Dim(0.45)]];
    [stack addArrangedSubview:[self label:@"If something looks off, tap this and paste it in a GitHub issue."
                                     size:11 weight:UIFontWeightRegular color:Dim(0.45)]];

    [stack addArrangedSubview:[self footer]];
    [stack addArrangedSubview:[self label:[NSString stringWithFormat:@"v%@ \u00B7 Shake again or tap outside to close", BF_VERSION]
                                     size:11 weight:UIFontWeightRegular color:Dim(0.35)]];

    CGFloat width = MIN(380, host.bounds.size.width - 24);
    self.cardCenterY = [card.centerYAnchor constraintEqualToAnchor:overlay.centerYAnchor];
    NSLayoutConstraint *fit = [scroll.heightAnchor constraintEqualToAnchor:stack.heightAnchor];
    fit.priority = UILayoutPriorityDefaultLow;
    [NSLayoutConstraint activateConstraints:@[
        [card.centerXAnchor constraintEqualToAnchor:overlay.centerXAnchor],
        self.cardCenterY,
        [card.widthAnchor constraintEqualToConstant:width],
        [card.heightAnchor constraintLessThanOrEqualToAnchor:overlay.safeAreaLayoutGuide.heightAnchor constant:-20],
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
    self.menuOverlay = overlay;
}

- (BOOL)menuVisible { return self.menuShowing; }

- (void)showMenu {
    UIWindow *w = HostWindow();
    if (!w) return;
    if (!self.menuOverlay) [self buildMenuIn:w];
    self.menuShowing = YES;
    self.menuOverlay.frame = w.bounds;
    [w addSubview:self.menuOverlay];
    [w bringSubviewToFront:self.menuOverlay];
    [self syncControls];
    [self refresh];
    self.menuOverlay.alpha = 0;
    [UIView animateWithDuration:0.18 animations:^{ self.menuOverlay.alpha = 1; }];
    [self.refreshTimer invalidate];
    self.refreshTimer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(refresh) userInfo:nil repeats:YES];
}

- (void)hideMenu {
    if (!self.menuShowing) return;
    self.menuShowing = NO;
    [self.refreshTimer invalidate];
    self.refreshTimer = nil;
    [self.searchField resignFirstResponder];
    UIView *o = self.menuOverlay;
    [UIView animateWithDuration:0.15 animations:^{ o.alpha = 0; } completion:^(BOOL finished) {
        if (!self.menuShowing) [o removeFromSuperview];
    }];
}

- (void)overlayTapped:(UITapGestureRecognizer *)g {
    CGPoint p = [g locationInView:self.card];
    if (![self.card pointInside:p withEvent:nil]) [self hideMenu];
}

- (void)keyboardChanged:(NSNotification *)n {
    UIView *o = self.menuOverlay;
    if (!self.menuShowing || !o.window) return;
    CGRect kb = [o convertRect:[n.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue] fromView:nil];
    CGFloat overlap = MAX(0, CGRectGetMaxY(o.bounds) - CGRectGetMinY(kb));
    self.cardCenterY.constant = -overlap / 2.0;
    [UIView animateWithDuration:0.25 animations:^{ [o layoutIfNeeded]; }];
}

- (void)keyboardHidden:(NSNotification *)n {
    self.cardCenterY.constant = 0;
    [UIView animateWithDuration:0.25 animations:^{ [self.menuOverlay layoutIfNeeded]; }];
}

- (void)skinChanged:(UISwitch *)s  { gBF.textureFix = s.on; BFSaveConfig(); }
- (void)pinChanged:(UISwitch *)s   { gBF.pinFix = s.on; BFSaveConfig(); }
- (void)specChanged:(UISwitch *)s  { gBF.pinSpec = s.on; BFSaveConfig(); }
- (void)unstickChanged:(UISwitch *)s { gBF.unstick = s.on; BFSaveConfig(); }
- (void)ipv4Changed:(UISwitch *)s  { gBF.gameIPv4 = s.on; BFSaveConfig(); }
- (void)fpsChanged:(UISwitch *)s   { gBF.fps120 = s.on; BFSaveConfig(); [self refresh]; }
- (void)privacyChanged:(UISwitch *)s { gBF.autoPrivacy = s.on; BFSaveConfig(); }
- (void)spareChanged:(UISwitch *)s { gBF.spareMode = s.on; BFSaveConfig(); [self refresh]; }
- (void)oilMirrorChanged:(UISwitch *)s { gBF.oilMirrorFix = s.on; BFSaveConfig(); }
- (void)oilBreakChanged:(UISwitch *)s  { gBF.oilBreakdown = s.on; BFSaveConfig(); }
- (void)oilThickChanged:(UISwitch *)s  { gBF.oilThickness = s.on; BFSaveConfig(); }
- (void)oilColorTapped { [self hideMenu]; BFOilShowColorPicker(); }
- (void)oilInvisChanged:(UISwitch *)s  { gBF.oilInvisible = s.on; BFSaveConfig(); [self refresh]; }
- (void)oilLibraryTapped { [self hideMenu]; BFOilShowLibrary(); }
- (void)autoChanged:(UISwitch *)s  { gBF.spareAuto = s.on; BFSaveConfig(); [self refresh]; }

- (void)copyLogTapped {
    [UIPasteboard generalPasteboard].string = [NSString stringWithFormat:@"%@\n%@", BFDebugInfo(), BFLogText()];
    [self.logButton setTitle:@"Copied!" forState:UIControlStateNormal];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self.logButton setTitle:@"Copy log" forState:UIControlStateNormal];
    });
}

- (void)netTestTapped {
    BFNetTest();
    [self.netButton setTitle:@"Testing (~20 s)..." forState:UIControlStateNormal];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(20 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self.netButton setTitle:@"Run connection test" forState:UIControlStateNormal];
    });
}

- (void)copyDebugTapped {
    [UIPasteboard generalPasteboard].string = BFDebugInfo();
    [self.debugButton setTitle:@"Copied! Paste it in your message" forState:UIControlStateNormal];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self.debugButton setTitle:@"Copy debug info" forState:UIControlStateNormal];
    });
}

- (void)speedChanged:(UISlider *)s {           // 1.0x ... 5.0x in 0.1 steps
    float m = roundf(s.value * 10.0f) / 10.0f;
    gBF.speedMult = fminf(fmaxf(m, 1.0f), BF_MAX_SPEED);
    [self updateSpeedLabel];
}
- (void)speedDone:(UISlider *)s { BFSaveConfig(); }
- (void)speedReset {
    gBF.speedMult = 1.0f;
    self.speedSlider.value = 1;
    [self updateSpeedLabel];
    BFSaveConfig();
}
- (void)updateSpeedLabel {
    float m = gBF.speedMult;
    self.speedLabel.text = [NSString stringWithFormat:@"%.1fx", m];
}

- (void)searchTapped {
    [self.searchField resignFirstResponder];
    BFSetArsenalQuery(self.searchField.text);
    [self hideMenu];
}
- (void)clearTapped {
    self.searchField.text = @"";
    [self.searchField resignFirstResponder];
    BFSetArsenalQuery(nil);
    [self refresh];
}
- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    [self searchTapped];
    return YES;
}

- (void)resumeTapped {
    BFExitSafeMode();
    [self refresh];
}

- (void)refresh {
    self.statusLabel.text = BFStatusLine();
    self.resumeButton.hidden = !gBFSafeMode;
    NSString *ball = BFBallLine();
    self.ballLabel.text = ball;
    self.ballLabel.hidden = ball.length == 0;
    self.arsenalLabel.text = BFArsenalLine();
    self.fpsLabel.text = BFFpsLine();
    NSString *oil = BFOilStatusLine();
    self.oilLabel.text = oil;
    self.oilLabel.hidden = oil.length == 0;
    self.autoSwitch.on = gBF.spareAuto;
    self.autoLabel.text = !gBF.spareAuto ? @"" : gBF.spareMode
        ? [NSString stringWithFormat:@"Auto-racking %@ every frame", BFPinsText(gBF.lastPinMask)]
        : @"Turn on Spare shooting mode to use Auto-rack";
    self.autoLabel.hidden = !gBF.spareAuto;
}

- (void)syncControls {
    self.skinSwitch.on = gBF.textureFix;
    self.pinSwitch.on = gBF.pinFix;
    self.specSwitch.on = gBF.pinSpec;
    self.unstickSwitch.on = gBF.unstick;
    self.ipv4Switch.on = gBF.gameIPv4;
    self.fpsSwitch.on = gBF.fps120;
    self.privacySwitch.on = gBF.autoPrivacy;
    self.oilMirrorSwitch.on = gBF.oilMirrorFix;
    self.oilBreakSwitch.on = gBF.oilBreakdown;
    self.oilInvisSwitch.on = gBF.oilInvisible;
    self.oilThickSwitch.on = gBF.oilThickness;
    self.oilColorButton.backgroundColor = gBF.oilHue < 0 ? [UIColor colorWithRed:0.86 green:0.70 blue:0.50 alpha:1]
                                                        : [UIColor colorWithHue:gBF.oilHue saturation:0.85 brightness:0.95 alpha:1];
    [self.oilColorButton setTitle:gBF.oilHue < 0 ? @"game" : @"" forState:UIControlStateNormal];
    self.oilColorButton.titleLabel.font = [UIFont systemFontOfSize:10 weight:UIFontWeightBold];
    [self.oilColorButton setTitleColor:[UIColor colorWithWhite:0.2 alpha:1] forState:UIControlStateNormal];
    self.spareSwitch.on = gBF.spareMode;
    self.autoSwitch.on = gBF.spareAuto;
    self.speedSlider.value = fminf(fmaxf(gBF.speedMult, 1.0f), BF_MAX_SPEED);
    [self updateSpeedLabel];
}

#pragma mark - spare mode pin picker

- (UIButton *)pinButton:(int)pin {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeCustom];
    b.tag = pin;
    [b setTitle:[NSString stringWithFormat:@"%d", pin] forState:UIControlStateNormal];
    b.titleLabel.font = [UIFont systemFontOfSize:17 weight:UIFontWeightBold];
    b.layer.cornerRadius = 23;
    b.layer.borderWidth = 2;
    b.translatesAutoresizingMaskIntoConstraints = NO;
    [b.widthAnchor constraintEqualToConstant:46].active = YES;
    [b.heightAnchor constraintEqualToConstant:46].active = YES;
    [b addTarget:self action:@selector(pinTapped:) forControlEvents:UIControlEventTouchUpInside];
    return b;
}

- (void)buildPickerIn:(UIView *)host {
    UIView *overlay = [[UIView alloc] initWithFrame:host.bounds];
    overlay.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    overlay.backgroundColor = [UIColor colorWithWhite:0 alpha:0.35];
    UIView *card = [self cardView];
    [overlay addSubview:card];
    UIStackView *stack = [self vstack];
    stack.spacing = 12;
    [card addSubview:stack];

    [stack addArrangedSubview:[self label:@"Spare mode: pick your pins" size:18 weight:UIFontWeightHeavy color:UIColor.whiteColor]];
    [stack addArrangedSubview:[self label:@"Tap pins to add or remove them, then hit Rack 'em. Auto racks the same pins every frame until you turn it off (shake for the menu). Scores in this mode are just for fun."
                                     size:12 weight:UIFontWeightRegular color:Dim(0.55)]];

    // the rack as you see it from the foul line: back row on top, head pin at the bottom
    self.pinButtons = [NSMutableArray array];
    for (int i = 0; i < 10; i++) [self.pinButtons addObject:[self pinButton:i + 1]];
    UIStackView *tri = [UIStackView new];
    tri.axis = UILayoutConstraintAxisVertical;
    tri.spacing = 10;
    tri.alignment = UIStackViewAlignmentCenter;
    NSArray<NSArray<NSNumber *> *> *rows = @[ @[@7, @8, @9, @10], @[@4, @5, @6], @[@2, @3], @[@1] ];
    for (NSArray<NSNumber *> *r in rows) {
        NSMutableArray<UIView *> *views = [NSMutableArray array];
        for (NSNumber *pin in r) [views addObject:self.pinButtons[pin.intValue - 1]];
        [tri addArrangedSubview:[self hstack:views spacing:14]];
    }
    [stack addArrangedSubview:tri];

    NSArray<NSString *> *presetTitles = @[ @"10 pin", @"7 pin", @"7-10", @"Bucket" ];
    NSMutableArray<UIView *> *presets = [NSMutableArray array];
    for (NSUInteger i = 0; i < presetTitles.count; i++) {
        UIButton *b = [self button:presetTitles[i] filled:NO small:YES action:@selector(presetTapped:)];
        b.tag = (NSInteger)i;
        [presets addObject:b];
    }
    UIStackView *presetRow = [self hstack:presets spacing:8];
    presetRow.distribution = UIStackViewDistributionFillEqually;
    [stack addArrangedSubview:presetRow];

    UIStackView *quick = [self hstack:@[[self button:@"All" filled:NO small:YES action:@selector(allTapped)],
                                        [self button:@"None" filled:NO small:YES action:@selector(noneTapped)],
                                        [self button:@"Full rack" filled:NO small:YES action:@selector(skipTapped)]] spacing:8];
    quick.distribution = UIStackViewDistributionFillEqually;
    [stack addArrangedSubview:quick];
    self.rackButton = [self button:@"Rack 'em" filled:YES small:NO action:@selector(rackTapped)];
    self.autoButton = [self button:@"Auto (every frame)" filled:NO small:NO action:@selector(autoTapped)];
    UIStackView *go = [self hstack:@[self.rackButton, self.autoButton] spacing:8];
    go.distribution = UIStackViewDistributionFillEqually;
    [stack addArrangedSubview:go];

    CGFloat width = MIN(360, host.bounds.size.width - 24);
    [NSLayoutConstraint activateConstraints:@[
        [card.centerXAnchor constraintEqualToAnchor:overlay.centerXAnchor],
        [card.bottomAnchor constraintEqualToAnchor:overlay.safeAreaLayoutGuide.bottomAnchor constant:-12],
        [card.widthAnchor constraintEqualToConstant:width],
        [stack.topAnchor constraintEqualToAnchor:card.topAnchor],
        [stack.bottomAnchor constraintEqualToAnchor:card.bottomAnchor],
        [stack.leadingAnchor constraintEqualToAnchor:card.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:card.trailingAnchor],
    ]];
    self.pickerOverlay = overlay;
}

- (void)updatePins {
    for (int i = 0; i < 10; i++) {
        UIButton *b = self.pinButtons[i];
        BOOL up = (self.pickerMask >> i) & 1;
        b.backgroundColor = up ? UIColor.whiteColor : UIColor.clearColor;
        b.layer.borderColor = (up ? UIColor.whiteColor : Dim(0.3)).CGColor;
        [b setTitleColor:(up ? [UIColor colorWithRed:0.78 green:0.12 blue:0.12 alpha:1] : Dim(0.4)) forState:UIControlStateNormal];
    }
    self.rackButton.enabled = self.autoButton.enabled = self.pickerMask != 0;
    self.rackButton.alpha = self.autoButton.alpha = self.pickerMask ? 1.0 : 0.4;
}

- (void)pinTapped:(UIButton *)b {
    self.pickerMask ^= (uint16_t)(1u << (b.tag - 1));
    [self updatePins];
}

- (void)presetTapped:(UIButton *)b {
    static const uint16_t masks[] = {
        1u << 9,                                       // 10 pin
        1u << 6,                                       // 7 pin
        (1u << 6) | (1u << 9),                         // 7-10 split
        (1u << 1) | (1u << 3) | (1u << 4) | (1u << 7)  // bucket 2-4-5-8
    };
    if (b.tag >= 0 && b.tag < 4) self.pickerMask = masks[b.tag];
    [self updatePins];
}

- (void)allTapped  { self.pickerMask = BF_ALL_PINS; [self updatePins]; }
- (void)noneTapped { self.pickerMask = 0; [self updatePins]; }
- (void)skipTapped { [self hidePicker]; BFApplySpareSelection(BF_ALL_PINS); }

- (void)rackTapped {
    if (!self.pickerMask) return;
    uint16_t m = self.pickerMask;
    [self hidePicker];
    BFApplySpareSelection(m);
}

- (void)autoTapped {
    if (!self.pickerMask) return;
    uint16_t m = self.pickerMask;
    gBF.spareAuto = true;
    BFSaveConfig();
    [self hidePicker];
    BFApplySpareSelection(m);
}

#pragma mark - skip tutorial button

static UIViewController *TopController(void) {
    UIViewController *vc = HostWindow().rootViewController;
    while (vc.presentedViewController) vc = vc.presentedViewController;
    return vc;
}

- (void)setSkipVisible:(BOOL)visible {
    UIWindow *w = HostWindow();
    if (!visible || !w) { [self.skipButton removeFromSuperview]; return; }
    if (!self.skipButton) {
        UIButton *b = [self button:@"\u23ED Skip tutorial" filled:YES small:NO action:@selector(skipTutorialTapped)];
        b.translatesAutoresizingMaskIntoConstraints = NO;
        b.contentEdgeInsets = UIEdgeInsetsMake(8, 14, 8, 14);
        b.layer.shadowColor = UIColor.blackColor.CGColor;
        b.layer.shadowOpacity = 0.45;
        b.layer.shadowRadius = 6;
        b.layer.shadowOffset = CGSizeMake(0, 2);
        self.skipButton = b;
    }
    if (self.skipButton.superview != w) {
        [self.skipButton removeFromSuperview];
        [w addSubview:self.skipButton];
        [NSLayoutConstraint activateConstraints:@[
            [self.skipButton.trailingAnchor constraintEqualToAnchor:w.safeAreaLayoutGuide.trailingAnchor constant:-12],
            [self.skipButton.topAnchor constraintEqualToAnchor:w.safeAreaLayoutGuide.topAnchor constant:8],
        ]];
    }
    [w bringSubviewToFront:self.skipButton];
}

- (void)skipTutorialTapped {
    UIAlertController *a = [UIAlertController alertControllerWithTitle:@"Skip the tutorial?"
        message:@"This uses the game's own skip function: the tutorial gets marked as done and you go to the main screen, where you can log in to your account."
        preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [a addAction:[UIAlertAction actionWithTitle:@"Skip" style:UIAlertActionStyleDefault handler:^(UIAlertAction *x) {
        [self setSkipVisible:NO];      // gone right away; the game won't show it again this session
        BFRequestSkipTutorial();
    }]];
    [TopController() presentViewController:a animated:YES completion:nil];
}

- (void)showPickerWithMask:(uint16_t)mask {
    UIWindow *w = HostWindow();
    if (!w) return;
    if (self.menuShowing) [self hideMenu];
    if (!self.pickerOverlay) [self buildPickerIn:w];
    self.pickerMask = (mask & BF_ALL_PINS) ? (mask & BF_ALL_PINS) : BF_ALL_PINS;
    [self updatePins];
    self.pickerOverlay.frame = w.bounds;
    [w addSubview:self.pickerOverlay];
    [w bringSubviewToFront:self.pickerOverlay];
    self.pickerOverlay.alpha = 0;
    [UIView animateWithDuration:0.18 animations:^{ self.pickerOverlay.alpha = 1; }];
}

- (void)hidePicker { [self.pickerOverlay removeFromSuperview]; }
- (BOOL)pickerVisible { return self.pickerOverlay.superview != nil; }

@end

void BFMenuToggle(void) {
    BFMenu *m = [BFMenu shared];
    if ([m pickerVisible]) return;     // finish picking pins first
    if ([m menuVisible]) [m hideMenu];
    else [m showMenu];
}
void BFMenuShowPinPicker(uint16_t mask) { [[BFMenu shared] showPickerWithMask:mask]; }
void BFMenuHidePinPicker(void) { [[BFMenu shared] hidePicker]; }
bool BFMenuPickerVisible(void) { return [[BFMenu shared] pickerVisible]; }
void BFMenuSetSkipTutorialVisible(bool visible) { [[BFMenu shared] setSkipVisible:visible]; }
