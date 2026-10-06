#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import "BFShared.h"
#import "Logo.h"
#include <math.h>

// The menu is plain UIKit drawn on top of the game. Nothing is on screen until you shake.
//
// Layout: one card per topic (collapsible), rows with a switch on the right. Descriptions stay hidden so the menu
// stays short: the (i) button in the header shows all of them, and tapping a row's title shows just that one.

static UIColor *Accent(void) { return [UIColor colorWithRed:1.0 green:0.48 blue:0.10 alpha:1.0]; }
static UIColor *Dim(CGFloat a) { return [UIColor colorWithWhite:1.0 alpha:a]; }
static UIColor *PanelColor(void) { return [UIColor colorWithWhite:1.0 alpha:0.055]; }
static const void *kHelpKey = &kHelpKey;          // associated-object key: a row title -> its description label

NSString *BFPinsText(uint16_t mask) {             // 0x240 -> "7-10"
    mask &= BF_ALL_PINS;
    if (mask == BF_ALL_PINS) return @"a full rack";
    NSMutableArray<NSString *> *pins = [NSMutableArray array];
    for (int i = 0; i < 10; i++) if (mask & (1u << i)) [pins addObject:[NSString stringWithFormat:@"%d", i + 1]];
    return pins.count ? [pins componentsJoinedByString:@"-"] : @"no pins";
}

static UIViewController *TopController(void);   // defined lower down; used by backupTapped

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
@property (nonatomic, strong) UIButton *updateButton, *helpButton;
@property (nonatomic, strong) UISwitch *pinImageSwitch;
@property (nonatomic, strong) UILabel *pinImageLabel;
@property (nonatomic, copy) NSString *updateURL;
@property (nonatomic, strong) UIButton *oilColorButton;
@property (nonatomic, strong) UISwitch *specSwitch, *fpsSwitch, *unstickSwitch, *ipv4Switch, *menuButtonSwitch;
@property (nonatomic, strong) UIButton *autoButton, *skipButton, *debugButton, *logButton, *netButton;
@property (nonatomic, strong) UISlider *speedSlider, *spinSlider;
@property (nonatomic, strong) UILabel *spinLabel;
@property (nonatomic, strong) UITextField *searchField;
@property (nonatomic, strong) NSTimer *refreshTimer;
@property (nonatomic, strong) NSMutableArray<UILabel *> *helpLabels;
@property (nonatomic) BOOL menuShowing;
@property (nonatomic, strong) UIView *pickerOverlay;
@property (nonatomic, strong) NSMutableArray<UIButton *> *pinButtons;
@property (nonatomic, strong) UIButton *rackButton;
@property (nonatomic, strong) UIButton *resumeButton;
@property (nonatomic, strong) UILabel *pickerTitle, *pickerHint;
@property (nonatomic) uint16_t pickerMask;
@property (nonatomic) BOOL pickerOneShot;
+ (instancetype)shared;
- (BOOL)menuVisible;
- (void)showMenu;
- (void)hideMenu;
- (void)showPickerWithMask:(uint16_t)mask oneShot:(BOOL)oneShot;
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

- (UIStackView *)hstack:(NSArray<UIView *> *)views spacing:(CGFloat)spacing {
    UIStackView *s = [[UIStackView alloc] initWithArrangedSubviews:views];
    s.axis = UILayoutConstraintAxisHorizontal;
    s.spacing = spacing;
    s.alignment = UIStackViewAlignmentCenter;
    return s;
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
    [b setTitleColor:filled ? [UIColor colorWithWhite:0.08 alpha:1] : UIColor.whiteColor forState:UIControlStateNormal];
    b.titleLabel.font = [UIFont systemFontOfSize:small ? 13 : 15 weight:UIFontWeightSemibold];
    b.titleLabel.adjustsFontSizeToFitWidth = YES;
    b.backgroundColor = filled ? Accent() : Dim(0.12);
    b.layer.cornerRadius = 10;
    b.contentEdgeInsets = small ? UIEdgeInsetsMake(8, 6, 8, 6) : UIEdgeInsetsMake(10, 14, 10, 14);
    [b addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return b;
}

- (UIButton *)linkButton:(NSString *)title url:(NSString *)url {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    NSDictionary *attrs = @{ NSFontAttributeName: [UIFont systemFontOfSize:13 weight:UIFontWeightBold],
                             NSForegroundColorAttributeName: Accent(),
                             NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle) };
    [b setAttributedTitle:[[NSAttributedString alloc] initWithString:title attributes:attrs] forState:UIControlStateNormal];
    if (url) [b addAction:[UIAction actionWithHandler:^(UIAction *a) {
        [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url] options:@{} completionHandler:nil];
    }] forControlEvents:UIControlEventTouchUpInside];
    return b;
}

#pragma mark - rows, cells and cards

// A row: title (and a dim (i) when it has a description), the control on the right. The description sits under
// it, hidden until you ask for it: the header's (i) shows all of them, tapping this title shows just this one.
- (UIView *)row:(NSString *)title help:(NSString *)help control:(UIView *)control {
    UIFont *tf = [UIFont systemFontOfSize:16 weight:UIFontWeightSemibold];
    UILabel *t = [self label:title size:16 weight:UIFontWeightSemibold color:UIColor.whiteColor];
    UILabel *h = nil;
    if (help.length) {
        NSMutableAttributedString *as = [[NSMutableAttributedString alloc] initWithString:title attributes:@{ NSFontAttributeName: tf, NSForegroundColorAttributeName: UIColor.whiteColor }];
        [as appendAttributedString:[[NSAttributedString alloc] initWithString:@"  \u24D8" attributes:@{ NSFontAttributeName: [UIFont systemFontOfSize:14 weight:UIFontWeightRegular], NSForegroundColorAttributeName: Dim(0.38) }]];
        t.attributedText = as;
        h = [self label:help size:12 weight:UIFontWeightRegular color:Dim(0.6)];
        h.hidden = !gBF.menuHelp;
        [self.helpLabels addObject:h];
        t.userInteractionEnabled = YES;
        UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(titleTapped:)];
        [t addGestureRecognizer:tap];
        objc_setAssociatedObject(t, kHelpKey, h, OBJC_ASSOCIATION_ASSIGN);
    }
    NSMutableArray *top = [NSMutableArray arrayWithObject:t];
    if (control) {
        [top addObject:control];
        [control setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        [control setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    }
    UIStackView *topRow = [self hstack:top spacing:12];
    UIStackView *v = [[UIStackView alloc] initWithArrangedSubviews:h ? @[topRow, h] : @[topRow]];
    v.axis = UILayoutConstraintAxisVertical;
    v.spacing = 6;
    return v;
}

- (void)titleTapped:(UITapGestureRecognizer *)g {
    UILabel *h = objc_getAssociatedObject(g.view, kHelpKey);
    if (!h) return;
    [UIView animateWithDuration:0.2 animations:^{
        h.hidden = !h.hidden;
        [self.card layoutIfNeeded];
    }];
}

- (void)helpToggled {
    gBF.menuHelp = !gBF.menuHelp;
    BFSaveConfig();
    [self syncHelpButton];
    [UIView animateWithDuration:0.2 animations:^{
        for (UILabel *h in self.helpLabels) h.hidden = !gBF.menuHelp;
        [self.card layoutIfNeeded];
    }];
}

- (void)syncHelpButton {
    UIImageSymbolConfiguration *cfg = [UIImageSymbolConfiguration configurationWithPointSize:17 weight:UIImageSymbolWeightSemibold];
    [self.helpButton setImage:[UIImage systemImageNamed:gBF.menuHelp ? @"info.circle.fill" : @"info.circle" withConfiguration:cfg] forState:UIControlStateNormal];
    self.helpButton.tintColor = gBF.menuHelp ? Accent() : Dim(0.7);
    self.helpButton.backgroundColor = gBF.menuHelp ? [Accent() colorWithAlphaComponent:0.16] : Dim(0.08);
}

// pads a row into a cell of a card
- (UIView *)cell:(UIView *)content {
    UIStackView *c = [[UIStackView alloc] initWithArrangedSubviews:@[content]];
    c.axis = UILayoutConstraintAxisVertical;
    c.layoutMargins = UIEdgeInsetsMake(12, 14, 12, 14);
    c.layoutMarginsRelativeArrangement = YES;
    return c;
}

- (UIView *)cellForLabel:(UILabel *)l {            // a small status line that disappears when empty
    return [self cell:l];
}

// A titled card holding cells. Tap its header to fold it.
- (UIView *)group:(NSString *)title icon:(NSString *)icon key:(NSString *)key open:(BOOL)defOpen cells:(NSArray<UIView *> *)cells {
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    NSString *dk = [@"BowlingPlus.menuOpen." stringByAppendingString:key];
    BOOL open = [ud objectForKey:dk] ? [ud boolForKey:dk] : defOpen;

    UILabel *head = [self label:[NSString stringWithFormat:@"%@  %@", icon, title.uppercaseString] size:12 weight:UIFontWeightBold color:Dim(0.6)];
    UILabel *chev = [self label:@"\u203A" size:22 weight:UIFontWeightBold color:Dim(0.45)];
    chev.transform = open ? CGAffineTransformMakeRotation(M_PI_2) : CGAffineTransformIdentity;
    [chev setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    UIStackView *headRow = [self hstack:@[head, chev] spacing:8];
    headRow.userInteractionEnabled = NO;
    headRow.translatesAutoresizingMaskIntoConstraints = NO;
    UIButton *headButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [headButton addSubview:headRow];
    [NSLayoutConstraint activateConstraints:@[
        [headRow.leadingAnchor constraintEqualToAnchor:headButton.leadingAnchor constant:4],
        [headRow.trailingAnchor constraintEqualToAnchor:headButton.trailingAnchor constant:-4],
        [headRow.topAnchor constraintEqualToAnchor:headButton.topAnchor constant:2],
        [headRow.bottomAnchor constraintEqualToAnchor:headButton.bottomAnchor constant:-2],
        [headButton.heightAnchor constraintGreaterThanOrEqualToConstant:30],
    ]];

    UIView *panel = [UIView new];
    panel.backgroundColor = PanelColor();
    panel.layer.cornerRadius = 14;
    panel.layer.borderWidth = 1;
    panel.layer.borderColor = Dim(0.06).CGColor;
    panel.clipsToBounds = YES;
    UIStackView *inner = [UIStackView new];
    inner.axis = UILayoutConstraintAxisVertical;
    inner.spacing = 0;
    inner.translatesAutoresizingMaskIntoConstraints = NO;
    for (NSUInteger i = 0; i < cells.count; i++) {
        if (i) {
            UIView *sep = [UIView new];
            sep.backgroundColor = Dim(0.07);
            [sep.heightAnchor constraintEqualToConstant:1].active = YES;
            [inner addArrangedSubview:sep];
        }
        [inner addArrangedSubview:cells[i]];
    }
    [panel addSubview:inner];
    [NSLayoutConstraint activateConstraints:@[
        [inner.topAnchor constraintEqualToAnchor:panel.topAnchor], [inner.bottomAnchor constraintEqualToAnchor:panel.bottomAnchor],
        [inner.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor], [inner.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor],
    ]];
    panel.hidden = !open;

    __weak BFMenu *weak = self;
    [headButton addAction:[UIAction actionWithHandler:^(UIAction *a) {
        BOOL nowOpen = panel.hidden;
        [ud setBool:nowOpen forKey:dk];
        [UIView animateWithDuration:0.22 animations:^{
            panel.hidden = !nowOpen;
            chev.transform = nowOpen ? CGAffineTransformMakeRotation(M_PI_2) : CGAffineTransformIdentity;
            [weak.card layoutIfNeeded];
        }];
    }] forControlEvents:UIControlEventTouchUpInside];

    UIStackView *g = [[UIStackView alloc] initWithArrangedSubviews:@[headButton, panel]];
    g.axis = UILayoutConstraintAxisVertical;
    g.spacing = 6;
    return g;
}

// title + live value + reset button, the slider, and a description
- (UIView *)sliderCell:(NSString *)title value:(UILabel *)value reset:(SEL)reset slider:(UISlider *)slider help:(NSString *)help {
    [value setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    UIButton *r = [self button:@"Reset" filled:NO small:YES action:reset];
    [r setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    UIView *head = [self row:title help:help control:nil];     // gives the (i) and the description
    UIStackView *headStack = (UIStackView *)head;
    UIStackView *topRow = (UIStackView *)headStack.arrangedSubviews.firstObject;
    [topRow addArrangedSubview:value];
    [topRow addArrangedSubview:r];
    UIStackView *v = [[UIStackView alloc] initWithArrangedSubviews:@[head, slider]];
    v.axis = UILayoutConstraintAxisVertical;
    v.spacing = 8;
    return [self cell:v];
}

- (UIView *)buttonCell:(UIButton *)b {
    return [self cell:b];
}

#pragma mark - the menu

// GitHub's "latest release" (the one marked Latest on the Releases page; drafts and pre-releases are
// skipped). The tag can be "1.4.3" or "v1.4.3".
static NSComparisonResult BPCompareVersions(NSString *a, NSString *b) {
    NSArray *x = [a componentsSeparatedByString:@"."], *y = [b componentsSeparatedByString:@"."];
    for (NSUInteger i = 0; i < MAX(x.count, y.count); i++) {
        int p = i < x.count ? [x[i] intValue] : 0, q = i < y.count ? [y[i] intValue] : 0;
        if (p != q) return p < q ? NSOrderedAscending : NSOrderedDescending;
    }
    return NSOrderedSame;
}

- (void)checkForUpdates {
    self.updateButton.hidden = NO;
    self.updateURL = nil;
    [self.updateButton setTitle:@"Checking GitHub\u2026" forState:UIControlStateNormal];
    [self.updateButton setTitleColor:Dim(0.55) forState:UIControlStateNormal];
    NSMutableURLRequest *r = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"https://api.github.com/repos/NicholasBly/BowlingPlus/releases/latest"]
                                                     cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:15];
    [r setValue:@"application/vnd.github+json" forHTTPHeaderField:@"Accept"];
    [r setValue:[NSString stringWithFormat:@"BowlingPlus/%@", BF_VERSION] forHTTPHeaderField:@"User-Agent"];
    __weak BFMenu *weak = self;
    [[NSURLSession.sharedSession dataTaskWithRequest:r completionHandler:^(NSData *d, NSURLResponse *resp, NSError *e) {
        NSDictionary *j = d ? [NSJSONSerialization JSONObjectWithData:d options:0 error:nil] : nil;
        NSString *tag = [j isKindOfClass:[NSDictionary class]] && [j[@"tag_name"] isKindOfClass:[NSString class]] ? j[@"tag_name"] : nil;
        NSString *page = [j isKindOfClass:[NSDictionary class]] && [j[@"html_url"] isKindOfClass:[NSString class]] ? j[@"html_url"]
                         : @"https://github.com/NicholasBly/BowlingPlus/releases/latest";
        dispatch_async(dispatch_get_main_queue(), ^{
            BFMenu *m = weak;
            if (!m) return;
            NSString *latest = [tag stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"vV "]];
            if (!latest.length) {
                [m.updateButton setTitle:@"Couldn't reach GitHub. Check your connection and try again." forState:UIControlStateNormal];
                [m.updateButton setTitleColor:Dim(0.55) forState:UIControlStateNormal];
            } else if (BPCompareVersions(latest, BF_VERSION) == NSOrderedDescending) {
                m.updateURL = page;
                [m.updateButton setTitle:[NSString stringWithFormat:@"BowlingPlus %@ is out (you have %@). Tap to download.", latest, BF_VERSION]
                                forState:UIControlStateNormal];
                [m.updateButton setTitleColor:Accent() forState:UIControlStateNormal];
            } else {
                [m.updateButton setTitle:[NSString stringWithFormat:@"You're up to date (%@).", BF_VERSION] forState:UIControlStateNormal];
                [m.updateButton setTitleColor:Dim(0.55) forState:UIControlStateNormal];
            }
        });
    }] resume];
}

- (void)openUpdate {
    if (self.updateURL) [[UIApplication sharedApplication] openURL:[NSURL URLWithString:self.updateURL] options:@{} completionHandler:nil];
}


// two lines so nothing can get cut off on a narrow screen:  logo  BowlingPlus  .  Donate   /   Check for updates
- (UIView *)footer {
    UIImageView *logo = [[UIImageView alloc] initWithImage:[UIImage imageWithData:[NSData dataWithBytes:kBPLogoPNG length:kBPLogoPNGLen] scale:3]];
    logo.contentMode = UIViewContentModeScaleAspectFit;
    [logo.widthAnchor constraintEqualToConstant:25].active = YES;
    [logo.heightAnchor constraintEqualToConstant:25].active = YES;
    UIButton *gh = [self linkButton:@"BowlingPlus" url:@"https://github.com/NicholasBly/BowlingPlus"];
    UIButton *donate = [self linkButton:@"\u2665 Donate" url:@"https://github.com/sponsors/NicholasBly"];
    UILabel *dot = [self label:@"\u00B7" size:15 weight:UIFontWeightBold color:Dim(0.35)];
    UIStackView *row1 = [self hstack:@[logo, gh, dot, donate] spacing:8];
    UIButton *updates = [self linkButton:@"Check for updates" url:nil];
    [updates addTarget:self action:@selector(checkForUpdates) forControlEvents:UIControlEventTouchUpInside];
    self.updateButton = [UIButton buttonWithType:UIButtonTypeSystem];       // what the check found (tap it to open the release)
    self.updateButton.titleLabel.font = [UIFont systemFontOfSize:12 weight:UIFontWeightSemibold];
    self.updateButton.titleLabel.numberOfLines = 0;
    self.updateButton.titleLabel.textAlignment = NSTextAlignmentCenter;
    self.updateButton.hidden = YES;
    [self.updateButton addTarget:self action:@selector(openUpdate) forControlEvents:UIControlEventTouchUpInside];
    UILabel *ver = [self label:[NSString stringWithFormat:@"v%@ \u00B7 Shake again or tap outside to close", BF_VERSION] size:11 weight:UIFontWeightRegular color:Dim(0.35)];
    ver.textAlignment = NSTextAlignmentCenter;
    UIStackView *col = [[UIStackView alloc] initWithArrangedSubviews:@[row1, updates, self.updateButton, ver]];
    col.axis = UILayoutConstraintAxisVertical;
    col.alignment = UIStackViewAlignmentCenter;
    col.spacing = 4;
    return col;
}

- (void)buildMenuIn:(UIView *)host {
    self.helpLabels = [NSMutableArray array];
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
    stack.spacing = 12;
    [scroll addSubview:stack];

    // header: title, the (i) that shows every description, close
    UIButton *close = [UIButton buttonWithType:UIButtonTypeSystem];
    [close setTitle:@"\u2715" forState:UIControlStateNormal];
    close.titleLabel.font = [UIFont systemFontOfSize:20 weight:UIFontWeightBold];
    [close setTitleColor:Dim(0.7) forState:UIControlStateNormal];
    [close addTarget:self action:@selector(hideMenu) forControlEvents:UIControlEventTouchUpInside];
    close.contentHorizontalAlignment = UIControlContentHorizontalAlignmentRight;
    [close.widthAnchor constraintEqualToConstant:44].active = YES;
    [close.heightAnchor constraintEqualToConstant:44].active = YES;
    self.helpButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.helpButton.layer.cornerRadius = 17;
    [self.helpButton.widthAnchor constraintEqualToConstant:34].active = YES;
    [self.helpButton.heightAnchor constraintEqualToConstant:34].active = YES;
    [self.helpButton addTarget:self action:@selector(helpToggled) forControlEvents:UIControlEventTouchUpInside];
    self.helpButton.accessibilityLabel = @"Show or hide the descriptions";
    [self syncHelpButton];
    UILabel *title = [self label:@"\U0001F3B3 BowlingPlus" size:22 weight:UIFontWeightHeavy color:UIColor.whiteColor];
    title.numberOfLines = 1;
    title.adjustsFontSizeToFitWidth = YES;
    [title setContentHuggingPriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    [self.helpButton setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [close setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [stack addArrangedSubview:[self hstack:@[title, self.helpButton, close] spacing:8]];
    self.statusLabel = [self label:@"" size:13 weight:UIFontWeightMedium color:Accent()];
    [stack addArrangedSubview:self.statusLabel];
    self.resumeButton = [self button:@"Turn BowlingPlus back on" filled:YES small:NO action:@selector(resumeTapped)];
    [stack addArrangedSubview:self.resumeButton];

    // ---- Arsenal search (near the top so the keyboard never covers it)
    UITextField *f = [UITextField new];
    f.attributedPlaceholder = [[NSAttributedString alloc] initWithString:@"Ball name, e.g. match up" attributes:@{ NSForegroundColorAttributeName: Dim(0.35) }];
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
    self.searchField = f;
    UIButton *go = [self button:@"Search" filled:YES small:NO action:@selector(searchTapped)];
    UIButton *clear = [self button:@"Clear" filled:NO small:NO action:@selector(clearTapped)];
    [f setContentHuggingPriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    [f setContentCompressionResistancePriority:UILayoutPriorityDefaultLow forAxis:UILayoutConstraintAxisHorizontal];
    for (UIButton *b in @[go, clear]) {
        [b setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
        [b setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    }
    self.arsenalLabel = [self label:@"" size:12 weight:UIFontWeightRegular color:Dim(0.55)];
    UIStackView *searchCell = [[UIStackView alloc] initWithArrangedSubviews:@[[self hstack:@[f, go, clear] spacing:8], self.arsenalLabel]];
    searchCell.axis = UILayoutConstraintAxisVertical;
    searchCell.spacing = 8;
    [stack addArrangedSubview:[self group:@"Arsenal search" icon:@"\U0001F50E" key:@"search" open:YES cells:@[[self cell:searchCell]]]];

    // ---- Practice fun
    self.speedLabel = [self label:@"" size:16 weight:UIFontWeightBold color:Accent()];
    UISlider *slider = [UISlider new];
    slider.minimumValue = 1;
    slider.maximumValue = BF_MAX_SPEED;
    slider.minimumTrackTintColor = Accent();
    [slider addTarget:self action:@selector(speedChanged:) forControlEvents:UIControlEventValueChanged];
    [slider addTarget:self action:@selector(speedDone:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside];
    self.speedSlider = slider;
    UIView *speedCell = [self sliderCell:@"Ball speed" value:self.speedLabel reset:@selector(speedReset) slider:slider
                                    help:@"Multiplies the ball's speed right after you let go of it. 1x to 5x."];

    self.spinLabel = [self label:@"" size:16 weight:UIFontWeightBold color:Accent()];
    UISlider *spin = [UISlider new];
    spin.minimumValue = 1;
    spin.maximumValue = BF_MAX_SPIN;
    spin.minimumTrackTintColor = Accent();
    [spin addTarget:self action:@selector(spinChanged:) forControlEvents:UIControlEventValueChanged];
    [spin addTarget:self action:@selector(speedDone:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside];
    self.spinSlider = spin;
    UIView *spinCell = [self sliderCell:@"Ball spin (RPM)" value:self.spinLabel reset:@selector(spinReset) slider:spin
                                   help:@"Multiplies your throw's spin right after release (the game caps it near 600 rpm). 17x takes a 600 rpm throw to about 10,000. The extra revs also add grip on the lane, so the ball hooks more. Very fast spin can look slow or backwards on screen (like car wheels in videos)."];

    self.spareSwitch = [self switchOn:gBF.spareMode action:@selector(spareChanged:)];
    self.autoSwitch = [self switchOn:gBF.spareAuto action:@selector(autoChanged:)];
    self.autoLabel = [self label:@"" size:12 weight:UIFontWeightSemibold color:Accent()];
    UIView *spareCell = [self cell:[self row:@"Spare shooting mode" help:@"Pick which pins stand at the start of every frame. Tip: you don't need this on to pick pins for one shot. Tap the pin layout (top right while you hold the ball, or the little screen under the ball return in the overhead view) and the pin picker opens just for that shot. Scores in this mode are just for fun."
                                     control:self.spareSwitch]];
    UIStackView *autoStack = [[UIStackView alloc] initWithArrangedSubviews:@[[self row:@"Auto-rack" help:@"Sets up the same pins every frame without asking. Tip: pick your pins once and tap Auto in the pin picker." control:self.autoSwitch], self.autoLabel]];
    autoStack.axis = UILayoutConstraintAxisVertical;
    autoStack.spacing = 6;
    [stack addArrangedSubview:[self group:@"Practice fun" icon:@"\U0001F3AF" key:@"fun" open:YES
                                    cells:@[speedCell, spinCell, spareCell, [self cell:autoStack]]]];

    // ---- Oil
    self.oilColorButton = [UIButton buttonWithType:UIButtonTypeCustom];
    self.oilColorButton.layer.cornerRadius = 15;
    self.oilColorButton.layer.borderWidth = 2;
    self.oilColorButton.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.35].CGColor;
    [self.oilColorButton.widthAnchor constraintEqualToConstant:44].active = YES;
    [self.oilColorButton.heightAnchor constraintEqualToConstant:30].active = YES;
    [self.oilColorButton addTarget:self action:@selector(oilColorTapped) forControlEvents:UIControlEventTouchUpInside];
    self.oilThickSwitch = [self switchOn:gBF.oilThickness action:@selector(oilThickChanged:)];
    self.oilBreakSwitch = [self switchOn:gBF.oilBreakdown action:@selector(oilBreakChanged:)];
    self.oilInvisSwitch = [self switchOn:gBF.oilInvisible action:@selector(oilInvisChanged:)];
    self.oilMirrorSwitch = [self switchOn:gBF.oilMirrorFix action:@selector(oilMirrorChanged:)];
    UILabel *always = [self label:@"ALWAYS ON" size:10 weight:UIFontWeightBold color:Accent()];
    always.numberOfLines = 1;
    self.oilLabel = [self label:@"" size:12 weight:UIFontWeightSemibold color:Accent()];
    UIStackView *libStack = [[UIStackView alloc] initWithArrangedSubviews:@[[self button:@"\U0001F6E2  Custom oil patterns" filled:NO small:NO action:@selector(oilLibraryTapped)], self.oilLabel]];
    libStack.axis = UILayoutConstraintAxisVertical;
    libStack.spacing = 8;
    [stack addArrangedSubview:[self group:@"Oil (practice)" icon:@"\U0001F6E2" key:@"oil" open:YES cells:@[
        [self cell:[self row:@"Real-life oil" help:@"In Practice, the game's own patterns and yours are drawn from their real Kegel data: exact distances, microliters per step, reverse oil adding on top, the brush carrying oil back to the foul line, and Kegel's brushed film over the whole lane, with left on the left. Online matches, tournaments and the tutorial always use the game's own oil." control:always]],
        [self cell:[self row:@"Oil color" help:@"Pick the color the lane shows oil in, or keep the game's." control:self.oilColorButton]],
        [self cell:[self row:@"Show oil thickness" help:@"Stronger shading by oil thickness (darker = more oil) instead of the game's look. Looks only, the ball feels the same." control:self.oilThickSwitch]],
        [self cell:[self row:@"Show oil breakdown" help:@"Redraws the lane oil after every shot so you can watch it break down over the game." control:self.oilBreakSwitch]],
        [self cell:[self row:@"Invisible oil" help:@"Hides the oil and plays a random unlocked game pattern each game. Read the lane like the real thing." control:self.oilInvisSwitch]],
        [self cell:[self row:@"Fix oil display side" help:@"The game drew the oil mirrored, so breakdown and carrydown showed up on the wrong side. Now they show where your ball went." control:self.oilMirrorSwitch]],
        [self cell:libStack],
    ]]];

    // ---- Fixes
    self.skinSwitch = [self switchOn:gBF.textureFix action:@selector(skinChanged:)];
    self.ballLabel = [self label:@"" size:12 weight:UIFontWeightRegular color:Dim(0.5)];
    UIStackView *skinStack = [[UIStackView alloc] initWithArrangedSubviews:@[[self row:@"Match Up skins" help:@"Fixes the Match Up Pearl/BP ball textures." control:self.skinSwitch], self.ballLabel]];
    skinStack.axis = UILayoutConstraintAxisVertical;
    skinStack.spacing = 6;
    self.pinSwitch = [self switchOn:gBF.pinFix action:@selector(pinChanged:)];
    self.specSwitch = [self switchOn:gBF.pinSpec action:@selector(specChanged:)];
    self.unstickSwitch = [self switchOn:gBF.unstick action:@selector(unstickChanged:)];
    self.ipv4Switch = [self switchOn:gBF.gameIPv4 action:@selector(ipv4Changed:)];
    [stack addArrangedSubview:[self group:@"Fixes" icon:@"\U0001F6E0" key:@"fixes" open:YES cells:@[
        [self cell:skinStack],
        [self cell:[self row:@"Pin physics fix" help:@"Fast pins can't fly through other pins, and pins clipped low at the base can tip over properly. Practice only." control:self.pinSwitch]],
        [self cell:[self row:@"Improve spinning pin collision (experimental)" help:@"Uses Unity's speculative collisions, which also predict spin." control:self.specSwitch]],
        [self cell:[self row:@"Fix connection" help:@"If loading sits on \"connecting\" for 30 s, shows the game's gray offline button. A loading circle stuck for 30 s gets hidden so you can try again." control:self.unstickSwitch]],
        [self cell:[self row:@"Game server over IPv4" help:@"The game always picks IPv6 when some DNS servers offer it, but its servers don't answer on IPv6, so it hangs indefinitely. This forces IPv4." control:self.ipv4Switch]],
    ]]];

    // ---- Pins & display
    self.fpsSwitch = [self switchOn:gBF.fps120 action:@selector(fpsChanged:)];
    self.fpsLabel = [self label:@"" size:12 weight:UIFontWeightRegular color:Dim(0.5)];
    UIStackView *fpsStack = [[UIStackView alloc] initWithArrangedSubviews:@[[self row:@"120 FPS mode" help:@"Runs menus and gameplay at 120 FPS on 120 Hz screens: smoother, with faster touch response. The game normally uses 30 FPS menus / 60 FPS play. Uses more battery." control:self.fpsSwitch], self.fpsLabel]];
    fpsStack.axis = UILayoutConstraintAxisVertical;
    fpsStack.spacing = 6;
    self.pinImageSwitch = [self switchOn:gBF.pinImage action:@selector(pinImageChanged:)];
    UIButton *photos = [self button:@"From Photos" filled:NO small:YES action:@selector(pinImageFromPhotos)];
    UIButton *files = [self button:@"From Files" filled:NO small:YES action:@selector(pinImageFromFiles)];
    UIStackView *pickRow = [self hstack:@[photos, files] spacing:8];
    pickRow.distribution = UIStackViewDistributionFillEqually;
    self.pinImageLabel = [self label:@"" size:12 weight:UIFontWeightSemibold color:Accent()];
    UIStackView *pinStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        [self row:@"Use my own pin image" help:@"Puts your own picture on the pins (all lanes). Draw on the wrap template: one sheet that wraps around the pin like paper, so there are no seams. Looks only." control:self.pinImageSwitch],
        pickRow, [self button:@"Get the wrap template + guide" filled:NO small:YES action:@selector(pinImageGuide)], self.pinImageLabel]];
    pinStack.axis = UILayoutConstraintAxisVertical;
    pinStack.spacing = 8;
    [stack addArrangedSubview:[self group:@"Pins & display" icon:@"\U0001F3A8" key:@"look" open:YES cells:@[[self cell:fpsStack], [self cell:pinStack]]]];

    // ---- Help & diagnostics (folded by default)
    self.debugButton = [self button:@"Copy debug info" filled:NO small:NO action:@selector(copyDebugTapped)];
    self.logButton = [self button:@"Copy log" filled:NO small:NO action:@selector(copyLogTapped)];
    self.netButton = [self button:@"Run connection test" filled:NO small:NO action:@selector(netTestTapped)];
    UIStackView *logRow = [self hstack:@[self.logButton, self.netButton] spacing:8];
    logRow.distribution = UIStackViewDistributionFillEqually;
    UILabel *dh = [self label:@"If something looks off, tap Copy debug info and paste it in a GitHub issue. The log records loading, the connection and network checks from the moment the game starts: run the connection test, wait ~20 s, then Copy log."
                         size:12 weight:UIFontWeightRegular color:Dim(0.6)];
    dh.hidden = !gBF.menuHelp;
    [self.helpLabels addObject:dh];
    UIStackView *diag = [[UIStackView alloc] initWithArrangedSubviews:@[self.debugButton, logRow, dh]];
    diag.axis = UILayoutConstraintAxisVertical;
    diag.spacing = 8;
    [stack addArrangedSubview:[self group:@"Help & diagnostics" icon:@"\U0001FA7A" key:@"help" open:NO cells:@[[self cell:diag]]]];

    // ---- Menu button (an alternative/addition to shaking)
    self.menuButtonSwitch = [self switchOn:gBF.menuButton action:@selector(menuButtonChanged:)];
    [stack addArrangedSubview:[self group:@"Menu button" icon:@"\U0001F518" key:@"menubtn" open:NO cells:@[
        [self cell:[self row:@"On-screen menu button" help:@"A small round button you can tap to open the menu, instead of shaking. Press and drag it to move it; it remembers where you put it. Shake keeps working too." control:self.menuButtonSwitch]]
    ]]];

    // ---- Back up my data
    UIButton *backup = [self button:@"\U0001F4E6  Back up my data" filled:YES small:NO action:@selector(backupTapped)];
    UILabel *bh = [self label:@"Saves everything the game keeps on this device - settings, save data, and cached Facebook login state - to one file you can keep in Files, AirDrop or email. A safety net if the game or its Facebook login ever stop working. Doesn't include anything that only lives on the game's servers."
                         size:12 weight:UIFontWeightRegular color:Dim(0.6)];
    bh.hidden = !gBF.menuHelp;
    [self.helpLabels addObject:bh];
    UIStackView *backupStack = [[UIStackView alloc] initWithArrangedSubviews:@[backup, bh]];
    backupStack.axis = UILayoutConstraintAxisVertical;
    backupStack.spacing = 8;
    [stack addArrangedSubview:[self group:@"Backup" icon:@"\U0001F4BE" key:@"backup" open:NO cells:@[[self cell:backupStack]]]];

    [stack addArrangedSubview:[self footer]];

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
    BFMenuButtonRefresh();   // hide the floating button while the menu itself is open
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
    BFMenuButtonRefresh();   // bring the floating button back, if it's turned on
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
- (void)menuButtonChanged:(UISwitch *)s { gBF.menuButton = s.on; BFSaveConfig(); BFMenuButtonRefresh(); }
- (void)fpsChanged:(UISwitch *)s   { gBF.fps120 = s.on; BFSaveConfig(); [self refresh]; }
- (void)spareChanged:(UISwitch *)s { gBF.spareMode = s.on; BFSaveConfig(); [self refresh]; }
- (void)oilMirrorChanged:(UISwitch *)s { gBF.oilMirrorFix = s.on; BFSaveConfig(); }
- (void)oilBreakChanged:(UISwitch *)s  { gBF.oilBreakdown = s.on; BFSaveConfig(); }
- (void)oilThickChanged:(UISwitch *)s  { gBF.oilThickness = s.on; BFSaveConfig(); }
- (void)pinImageChanged:(UISwitch *)s {
    gBF.pinImage = s.on;
    BFSaveConfig();
    if (s.on && ![[NSFileManager defaultManager] fileExistsAtPath:BFPinImagePath()]) [self pinImageFromPhotos];   // nothing picked yet
    [self refresh];
}
- (void)pinImagePicked:(NSString *)message {
    [self refresh];
    self.pinImageLabel.text = message;
}
- (void)pinImageFromPhotos {
    __weak BFMenu *weak = self;
    BFPinImagePick(NO, ^(NSString *m) { [weak pinImagePicked:m]; });
}
- (void)pinImageFromFiles {
    __weak BFMenu *weak = self;
    BFPinImagePick(YES, ^(NSString *m) { [weak pinImagePicked:m]; });
}
- (void)pinImageGuide { BFPinImageShareGuide(); }
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

- (void)backupTapped {
    NSString *path = BFWriteBackup();
    if (!path) {
        UIAlertController *a = [UIAlertController alertControllerWithTitle:@"Backup failed" message:@"Couldn't write the backup file." preferredStyle:UIAlertControllerStyleAlert];
        [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
        [TopController() presentViewController:a animated:YES completion:nil];
        return;
    }
    NSURL *url = [NSURL fileURLWithPath:path];
    UIActivityViewController *avc = [[UIActivityViewController alloc] initWithActivityItems:@[url] applicationActivities:nil];
    UIViewController *top = TopController();
    avc.popoverPresentationController.sourceView = top.view;    // iPad: anchor the share sheet
    avc.popoverPresentationController.sourceRect = CGRectMake(top.view.bounds.size.width / 2, top.view.bounds.size.height / 2, 1, 1);
    [top presentViewController:avc animated:YES completion:nil];
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
- (void)spinChanged:(UISlider *)s {            // 1x ... 17x in 0.5 steps
    float m = roundf(s.value * 2.0f) / 2.0f;
    gBF.spinMult = fminf(fmaxf(m, 1.0f), BF_MAX_SPIN);
    [self updateSpeedLabel];
}
- (void)spinReset {
    gBF.spinMult = 1.0f;
    self.spinSlider.value = 1;
    [self updateSpeedLabel];
    BFSaveConfig();
}
- (void)updateSpeedLabel {
    self.spinLabel.text = gBF.spinMult < 1.01f ? @"1x" : [NSString stringWithFormat:@"%.1fx (600 \u2192 %d)", gBF.spinMult, (int)lroundf(600 * gBF.spinMult)];
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
    self.arsenalLabel.hidden = self.arsenalLabel.text.length == 0;
    self.fpsLabel.hidden = self.fpsLabel.text.length == 0;
    self.pinImageLabel.hidden = self.pinImageLabel.text.length == 0;
}

- (void)syncControls {
    [self syncHelpButton];
    for (UILabel *h in self.helpLabels) h.hidden = !gBF.menuHelp;
    self.skinSwitch.on = gBF.textureFix;
    self.pinSwitch.on = gBF.pinFix;
    self.specSwitch.on = gBF.pinSpec;
    self.unstickSwitch.on = gBF.unstick;
    self.ipv4Switch.on = gBF.gameIPv4;
    self.menuButtonSwitch.on = gBF.menuButton;
    self.fpsSwitch.on = gBF.fps120;
    self.oilMirrorSwitch.on = gBF.oilMirrorFix;
    self.oilBreakSwitch.on = gBF.oilBreakdown;
    self.oilInvisSwitch.on = gBF.oilInvisible;
    self.oilThickSwitch.on = gBF.oilThickness;
    self.pinImageSwitch.on = gBF.pinImage;
    self.pinImageLabel.text = BFPinImageStatus();
    self.oilColorButton.backgroundColor = gBF.oilHue < 0 ? [UIColor colorWithRed:0.86 green:0.70 blue:0.50 alpha:1]
                                                        : [UIColor colorWithHue:gBF.oilHue saturation:0.85 brightness:0.95 alpha:1];
    [self.oilColorButton setTitle:gBF.oilHue < 0 ? @"game" : @"" forState:UIControlStateNormal];
    self.oilColorButton.titleLabel.font = [UIFont systemFontOfSize:10 weight:UIFontWeightBold];
    [self.oilColorButton setTitleColor:[UIColor colorWithWhite:0.2 alpha:1] forState:UIControlStateNormal];
    self.spareSwitch.on = gBF.spareMode;
    self.autoSwitch.on = gBF.spareAuto;
    self.speedSlider.value = fminf(fmaxf(gBF.speedMult, 1.0f), BF_MAX_SPEED);
    self.spinSlider.value = fminf(fmaxf(gBF.spinMult, 1.0f), BF_MAX_SPIN);
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

    UIButton *x = [UIButton buttonWithType:UIButtonTypeSystem];            // leaves without changing anything
    [x setTitle:@"\u2715" forState:UIControlStateNormal];
    x.titleLabel.font = [UIFont systemFontOfSize:20 weight:UIFontWeightBold];
    [x setTitleColor:Dim(0.7) forState:UIControlStateNormal];
    x.contentHorizontalAlignment = UIControlContentHorizontalAlignmentRight;
    [x.widthAnchor constraintEqualToConstant:44].active = YES;
    [x.heightAnchor constraintEqualToConstant:36].active = YES;
    [x addTarget:self action:@selector(pickerClosed) forControlEvents:UIControlEventTouchUpInside];
    x.accessibilityLabel = @"Close without changes";
    [x setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    self.pickerTitle = [self label:@"Spare mode: pick your pins" size:18 weight:UIFontWeightHeavy color:UIColor.whiteColor];
    self.pickerTitle.numberOfLines = 1;
    self.pickerTitle.adjustsFontSizeToFitWidth = YES;
    [stack addArrangedSubview:[self hstack:@[self.pickerTitle, x] spacing:8]];
    self.pickerHint = [self label:@"" size:12 weight:UIFontWeightRegular color:Dim(0.55)];
    [stack addArrangedSubview:self.pickerHint];

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

- (void)skipTapped {                                   // a full rack
    BOOL oneShot = self.pickerOneShot;
    [self hidePicker];
    if (oneShot) BFApplySpareSelectionNow(BF_ALL_PINS); else BFApplySpareSelection(BF_ALL_PINS);
}

- (void)rackTapped {
    if (!self.pickerMask) return;
    uint16_t m = self.pickerMask;
    BOOL oneShot = self.pickerOneShot;
    [self hidePicker];
    if (oneShot) BFApplySpareSelectionNow(m); else BFApplySpareSelection(m);
}

- (void)autoTapped {
    if (!self.pickerMask || self.pickerOneShot) return;
    uint16_t m = self.pickerMask;
    gBF.spareAuto = true;
    BFSaveConfig();
    [self hidePicker];
    BFApplySpareSelection(m);
}

- (void)pickerClosed {                                 // the X: nothing changes
    [self hidePicker];
    BFSpareDismissed();
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

- (void)showPickerWithMask:(uint16_t)mask oneShot:(BOOL)oneShot {
    UIWindow *w = HostWindow();
    if (!w) return;
    if (self.menuShowing) [self hideMenu];
    if (!self.pickerOverlay) [self buildPickerIn:w];
    self.pickerOneShot = oneShot;
    self.pickerTitle.text = oneShot ? @"Pick your pins" : @"Spare mode: pick your pins";
    self.pickerHint.text = oneShot
        ? @"Tap pins to add or remove them, then hit Rack 'em. This is just for this shot: next frame is a full rack again. Turn on Spare shooting mode in the menu to be asked every frame. Scores are just for fun."
        : @"Tap pins to add or remove them, then hit Rack 'em. Auto racks the same pins every frame until you turn it off (shake for the menu). The X leaves everything as it is. Scores in this mode are just for fun.";
    self.autoButton.hidden = oneShot;
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

// ---- tapping the game's pin layouts (see Game.mm, BFPinTapAt) ----
@interface BFTapListener : NSObject <UIGestureRecognizerDelegate>
@end
@implementation BFTapListener
- (void)tapped:(UITapGestureRecognizer *)g {
    if (g.state != UIGestureRecognizerStateEnded) return;
    UIView *w = g.view;
    CGPoint p = [g locationInView:w];
    if (w.bounds.size.width < 1 || w.bounds.size.height < 1) return;
    BFPinTapAt((float)(p.x / w.bounds.size.width), (float)(1.0 - p.y / w.bounds.size.height));
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)g shouldReceiveTouch:(UITouch *)t {
    return !([[BFMenu shared] menuVisible] || [[BFMenu shared] pickerVisible]);
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)g shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)o { return YES; }
@end

static BFTapListener *sTapListener;
static UITapGestureRecognizer *sTapRecognizer;
static __weak UIWindow *sTapWindow;

void BFPinTapInstall(void) {
    static NSTimer *timer;
    if (timer) return;
    sTapListener = [BFTapListener new];
    timer = [NSTimer scheduledTimerWithTimeInterval:1.0 repeats:YES block:^(NSTimer *t) {
        id d = [UIApplication sharedApplication].delegate;
        UIWindow *w = [d respondsToSelector:@selector(window)] ? [d window] : HostWindow();   // the game's own window
        if (!w || w == sTapWindow) return;
        [sTapRecognizer.view removeGestureRecognizer:sTapRecognizer];
        sTapRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:sTapListener action:@selector(tapped:)];
        sTapRecognizer.cancelsTouchesInView = NO;      // the game still gets every touch
        sTapRecognizer.delaysTouchesBegan = NO;
        sTapRecognizer.delaysTouchesEnded = NO;
        sTapRecognizer.delegate = sTapListener;
        [w addGestureRecognizer:sTapRecognizer];
        sTapWindow = w;
    }];
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

void BFMenuToggle(void) {
    BFMenu *m = [BFMenu shared];
    if ([m pickerVisible]) return;     // finish picking pins first
    if ([m menuVisible]) [m hideMenu];
    else [m showMenu];
}
void BFMenuShowPinPicker(uint16_t mask) { [[BFMenu shared] showPickerWithMask:mask oneShot:NO]; }
void BFMenuShowPinPickerOneShot(uint16_t mask) { [[BFMenu shared] showPickerWithMask:mask oneShot:YES]; }
void BFMenuHidePinPicker(void) { [[BFMenu shared] hidePicker]; }
bool BFMenuPickerVisible(void) { return [[BFMenu shared] pickerVisible]; }
bool BFMenuPickerOneShot(void) { return [[BFMenu shared] pickerVisible] && [BFMenu shared].pickerOneShot; }
bool BFMenuVisible(void) { return [[BFMenu shared] menuVisible]; }
void BFMenuSetSkipTutorialVisible(bool visible) { [[BFMenu shared] setSkipVisible:visible]; }
