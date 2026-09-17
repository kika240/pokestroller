#import <Cocoa/Cocoa.h>
#include <sys/stat.h>
#include "walker.h"

// All emulation and AppKit callbacks run on the main thread. No shared core state.
@interface WalkerDisplay : NSView
@property(nonatomic, strong) NSBitmapImageRep *bitmap;
- (void)updateFrame;
@end

@implementation WalkerDisplay
- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
            pixelsWide:LCD_WIDTH pixelsHigh:LCD_HEIGHT bitsPerSample:8 samplesPerPixel:3
            hasAlpha:NO isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
            bytesPerRow:LCD_WIDTH * 3 bitsPerPixel:24];
        memset(_bitmap.bitmapData, 0xcc, LCD_WIDTH * LCD_HEIGHT * 3);
        self.accessibilityLabel = @"Écran du PokéWalker";
    }
    return self;
}
- (void)updateFrame {
    uint32_t pixels[LCD_WIDTH * LCD_HEIGHT];
    fillVideoBuffer(pixels);
    unsigned char *out = self.bitmap.bitmapData;
    for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        out[i * 3] = (pixels[i] >> 16) & 0xff;
        out[i * 3 + 1] = (pixels[i] >> 8) & 0xff;
        out[i * 3 + 2] = pixels[i] & 0xff;
    }
    self.needsDisplay = YES;
}
- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    [[NSColor colorWithWhite:0.12 alpha:1] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:self.bounds xRadius:12 yRadius:12] fill];
    // Integer pixel scaling: the original 96 x 64 LCD remains crisp on Retina.
    CGFloat scale = MAX(1, floor(MIN((self.bounds.size.width - 24) / LCD_WIDTH,
                                    (self.bounds.size.height - 24) / LCD_HEIGHT)));
    NSRect screen = NSMakeRect(floor((self.bounds.size.width - LCD_WIDTH * scale) / 2),
                               floor((self.bounds.size.height - LCD_HEIGHT * scale) / 2),
                               LCD_WIDTH * scale, LCD_HEIGHT * scale);
    NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
    [self.bitmap drawInRect:screen];
}
@end

static BOOL sameFile(NSURL *first, NSURL *second) {
    if (!first || !second) return NO;
    NSString *a = first.URLByResolvingSymlinksInPath.URLByStandardizingPath.path;
    NSString *b = second.URLByResolvingSymlinksInPath.URLByStandardizingPath.path;
    if ([a isEqualToString:b]) return YES;
    struct stat sa, sb;
    return stat(a.fileSystemRepresentation, &sa) == 0 &&
           stat(b.fileSystemRepresentation, &sb) == 0 &&
           sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

@interface WalkerApp : NSObject <NSApplicationDelegate, NSWindowDelegate, NSMenuItemValidation>
@property(nonatomic, strong) NSWindow *window;
@property(nonatomic, strong) WalkerDisplay *display;
@property(nonatomic, strong) NSTextField *status;
@property(nonatomic, strong) NSButton *pauseButton;
@property(nonatomic, strong) NSArray<NSButton *> *controls;
@property(nonatomic, strong) NSTimer *timer;
@property(nonatomic, strong) id eventMonitor;
@property(nonatomic, strong) NSURL *romURL;
@property(nonatomic, strong) NSURL *eepromURL;
@property(nonatomic, strong) NSMutableArray<NSURL *> *inputURLs;
@property(nonatomic, strong) NSData *savedEEPROM;
@property(nonatomic) BOOL loaded;
@property(nonatomic) BOOL paused;
@property(nonatomic) BOOL failed;
@property(nonatomic) uint64_t cycles;
@property(nonatomic) uint64_t targetCycles;
@property(nonatomic) uint64_t nextQuarter;
@end

@implementation WalkerApp
- (NSMenuItem *)item:(NSString *)title action:(SEL)action key:(NSString *)key {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];
    item.target = self;
    return item;
}
- (void)makeMenu {
    NSMenu *bar = [[NSMenu alloc] init];
    NSMenuItem *appItem = [bar addItemWithTitle:@"PokeStroller" action:NULL keyEquivalent:@""];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"PokeStroller"];
    [appMenu addItem:[self item:@"À propos de PokeStroller" action:@selector(about:) key:@""]];
    [appMenu addItem:NSMenuItem.separatorItem];
    NSMenuItem *quit = [appMenu addItemWithTitle:@"Quitter PokeStroller" action:@selector(terminate:) keyEquivalent:@"q"];
    quit.target = NSApp;
    appItem.submenu = appMenu;
    NSMenuItem *fileItem = [bar addItemWithTitle:@"Fichier" action:NULL keyEquivalent:@""];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"Fichier"];
    [fileMenu addItem:[self item:@"Ouvrir ROM et EEPROM…" action:@selector(openImages:) key:@"o"]];
    [fileMenu addItem:[self item:@"Exporter l’EEPROM…" action:@selector(exportEEPROM:) key:@"s"]];
    [fileMenu addItem:NSMenuItem.separatorItem];
    NSMenuItem *close = [fileMenu addItemWithTitle:@"Fermer" action:@selector(performClose:) keyEquivalent:@"w"];
    close.target = nil;
    fileItem.submenu = fileMenu;
    // Standard responder-chain actions also enable editing in native file panels.
    NSMenuItem *editItem = [bar addItemWithTitle:@"Édition" action:NULL keyEquivalent:@""];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Édition"];
    [editMenu addItemWithTitle:@"Annuler" action:@selector(undo:) keyEquivalent:@"z"];
    NSMenuItem *redo = [editMenu addItemWithTitle:@"Rétablir" action:@selector(redo:) keyEquivalent:@"z"];
    redo.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [editMenu addItem:NSMenuItem.separatorItem];
    [editMenu addItemWithTitle:@"Couper" action:@selector(cut:) keyEquivalent:@"x"];
    [editMenu addItemWithTitle:@"Copier" action:@selector(copy:) keyEquivalent:@"c"];
    [editMenu addItemWithTitle:@"Coller" action:@selector(paste:) keyEquivalent:@"v"];
    [editMenu addItemWithTitle:@"Tout sélectionner" action:@selector(selectAll:) keyEquivalent:@"a"];
    editItem.submenu = editMenu;
    NSMenuItem *emulatorItem = [bar addItemWithTitle:@"Émulation" action:NULL keyEquivalent:@""];
    NSMenu *emulatorMenu = [[NSMenu alloc] initWithTitle:@"Émulation"];
    [emulatorMenu addItem:[self item:@"Pause / Reprendre" action:@selector(togglePause:) key:@"p"]];
    [emulatorMenu addItem:[self item:@"Recharger les fichiers" action:@selector(restart:) key:@"r"]];
    emulatorItem.submenu = emulatorMenu;
    NSApp.mainMenu = bar;
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    [self makeMenu];
    self.inputURLs = [NSMutableArray array];
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 520, 490)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"PokeStroller";
    self.window.contentMinSize = NSMakeSize(440, 414);
    self.window.delegate = self;
    self.window.releasedWhenClosed = NO;
    NSView *content = self.window.contentView;
    NSTextField *heading = [NSTextField labelWithString:@"PokeStroller"];
    heading.font = [NSFont systemFontOfSize:24 weight:NSFontWeightSemibold];
    NSButton *openButton = [NSButton buttonWithTitle:@"Ouvrir…" target:self action:@selector(openImages:)];
    self.display = [[WalkerDisplay alloc] initWithFrame:NSZeroRect];
    self.status = [NSTextField labelWithString:@"Ouvrez votre ROM, puis votre EEPROM."];
    self.status.alignment = NSTextAlignmentCenter;
    self.status.font = [NSFont systemFontOfSize:12];
    self.status.textColor = NSColor.secondaryLabelColor;
    self.status.lineBreakMode = NSLineBreakByTruncatingMiddle;
    NSButton *left = [NSButton buttonWithTitle:@"◀  Z / ←" target:self action:@selector(pressKey:)];
    left.tag = LEFT;
    NSButton *enter = [NSButton buttonWithTitle:@"●  Espace" target:self action:@selector(pressKey:)];
    enter.tag = ENTER;
    NSButton *right = [NSButton buttonWithTitle:@"X / →  ▶" target:self action:@selector(pressKey:)];
    right.tag = RIGHT;
    self.controls = @[left, enter, right];
    for (NSButton *button in self.controls) button.enabled = NO;
    self.pauseButton = [NSButton buttonWithTitle:@"Pause" target:self action:@selector(togglePause:)];
    self.pauseButton.enabled = NO;
    NSStackView *buttons = [NSStackView stackViewWithViews:@[left, enter, right, self.pauseButton]];
    buttons.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    buttons.spacing = 10;
    for (NSView *view in @[heading, openButton, self.display, buttons, self.status]) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [content addSubview:view];
    }
    [NSLayoutConstraint activateConstraints:@[
        [heading.topAnchor constraintEqualToAnchor:content.topAnchor constant:22],
        [heading.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:24],
        [openButton.centerYAnchor constraintEqualToAnchor:heading.centerYAnchor],
        [openButton.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-24],
        [self.display.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:18],
        [self.display.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:24],
        [self.display.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-24],
        [self.display.bottomAnchor constraintEqualToAnchor:buttons.topAnchor constant:-18],
        [buttons.centerXAnchor constraintEqualToAnchor:content.centerXAnchor],
        [buttons.bottomAnchor constraintEqualToAnchor:self.status.topAnchor constant:-14],
        [self.status.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:24],
        [self.status.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-24],
        [self.status.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-20]
    ]];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    __weak WalkerApp *weakSelf = self;
    self.eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
        handler:^NSEvent *(NSEvent *event) {
            WalkerApp *app = weakSelf;
            if (!app.loaded || app.paused || app.failed || event.window != app.window ||
                NSApp.modalWindow || (event.modifierFlags & (NSEventModifierFlagCommand |
                                                           NSEventModifierFlagControl |
                                                           NSEventModifierFlagOption))) return event;
            NSString *key = event.charactersIgnoringModifiers.lowercaseString;
            uint8_t input = 0;
            if ([key isEqualToString:@"z"] || event.keyCode == 123) input = LEFT;
            else if ([key isEqualToString:@"x"] || event.keyCode == 124) input = RIGHT;
            else if ([key isEqualToString:@" "] || event.keyCode == 36) input = ENTER;
            if (!input) return event;
            if (!event.isARepeat) setKeys(input);
            return nil;
        }];
    self.timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60 target:self
                  selector:@selector(tick:) userInfo:nil repeats:YES];
    self.timer.tolerance = 0.001;
    // Explicit paths work from Terminal and `open --args`, regardless of cwd.
    NSArray<NSString *> *args = NSProcessInfo.processInfo.arguments;
    NSString *rom = nil, *eeprom = nil;
    for (NSUInteger i = 1; i < args.count; i++) {
        if ([args[i] isEqualToString:@"--rom"] && i + 1 < args.count) rom = args[++i];
        else if ([args[i] isEqualToString:@"--eeprom"] && i + 1 < args.count) eeprom = args[++i];
    }
    if (rom && eeprom) {
        [self loadROM:[NSURL fileURLWithPath:rom] eeprom:[NSURL fileURLWithPath:eeprom]];
    } else if (rom || eeprom) {
        [self showError:@"Indiquez les deux chemins : --rom fichier --eeprom fichier."];
    }
}
- (void)showError:(NSString *)message {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"PokeStroller";
    alert.informativeText = message;
    alert.alertStyle = NSAlertStyleWarning;
    [alert runModal];
}
- (NSData *)currentEEPROM {
    if (!self.loaded) return nil;
    NSMutableData *data = [NSMutableData dataWithLength:WALKER_EEPROM_SIZE];
    return copyWalkerEEPROM(data.mutableBytes, data.length) ? data : nil;
}
- (BOOL)confirmDiscard {
    if (!self.loaded || [[self currentEEPROM] isEqual:self.savedEEPROM]) return YES;
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Conserver les modifications de cette session ?";
    alert.informativeText = @"Votre EEPROM d’origine reste intacte. Exportez la session pour la reprendre plus tard.";
    [alert addButtonWithTitle:@"Exporter…"];
    [alert addButtonWithTitle:@"Abandonner la session"];
    [alert addButtonWithTitle:@"Annuler"];
    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) return [self writeEEPROM];
    return response == NSAlertSecondButtonReturn;
}
- (NSURL *)chooseFile:(NSString *)message {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.message = message;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    return [panel runModal] == NSModalResponseOK ? panel.URL : nil;
}
- (void)openImages:(id)sender {
    (void)sender;
    if (![self confirmDiscard]) return;
    NSURL *rom = [self chooseFile:@"Sélectionnez la ROM du PokéWalker (48 Kio, ou dump mémoire de 64 Kio)."];
    if (!rom) return;
    NSURL *eeprom = [self chooseFile:@"Sélectionnez l’EEPROM du PokéWalker (64 Kio). Elle sera lue sans être modifiée."];
    if (eeprom) [self loadROM:rom eeprom:eeprom];
}
- (void)loadROM:(NSURL *)rom eeprom:(NSURL *)eeprom {
    char error[2048];
    if (!initWalkerFromFiles(rom.fileSystemRepresentation, eeprom.fileSystemRepresentation,
                              error, sizeof(error))) {
        [self showError:[NSString stringWithUTF8String:error]];
        return;
    }
    self.romURL = rom;
    self.eepromURL = eeprom;
    // Protect every input opened in this app session, including a previous pair.
    [self.inputURLs addObjectsFromArray:@[rom, eeprom]];
    self.cycles = 0;
    self.targetCycles = 0;
    self.nextQuarter = SYSTEM_CLOCK_CYCLES_PER_SECOND / 4;
    self.loaded = YES;
    self.failed = NO;
    self.paused = NO;
    self.savedEEPROM = [self currentEEPROM];
    self.window.title = [@"PokeStroller — " stringByAppendingString:eeprom.lastPathComponent];
    [self.display updateFrame];
    [self updateControls];
}
- (void)updateControls {
    for (NSButton *button in self.controls) button.enabled = self.loaded && !self.paused && !self.failed;
    self.pauseButton.enabled = self.loaded && !self.failed;
    self.pauseButton.title = self.paused ? @"Reprendre" : @"Pause";
    if (self.loaded && !self.failed) self.status.stringValue = self.paused ? @"En pause" : @"Espace : réveiller / valider · Z / X ou ← / → : naviguer";
}
- (void)tick:(NSTimer *)timer {
    (void)timer;
    if (!self.loaded || self.paused || self.failed || NSApp.modalWindow) return;
    // Deliberately do not catch up time spent asleep or in a file dialog.
    self.targetCycles += SYSTEM_CLOCK_CYCLES_PER_SECOND / 60;
    uint64_t cycleCount = self.cycles;
    while (cycleCount < self.targetCycles) {
        if (runNextInstruction(&cycleCount)) {
            self.failed = YES;
            [self updateControls];
            self.status.stringValue = [NSString stringWithFormat:@"Émulation arrêtée à 0x%04X", walkerProgramCounter()];
            [self showError:[NSString stringWithFormat:@"Le cœur expérimental a rencontré une opération non prise en charge à 0x%04X. Vous pouvez exporter l’EEPROM ou recharger les fichiers.", walkerProgramCounter()]];
            break;
        }
        if (cycleCount >= self.nextQuarter) {
            quarterRTCInterrupt();
            self.nextQuarter += SYSTEM_CLOCK_CYCLES_PER_SECOND / 4;
            [self.display updateFrame];
        }
    }
    self.cycles = cycleCount;
}
- (void)pressKey:(NSButton *)sender {
    if (self.loaded && !self.paused && !self.failed) setKeys((uint8_t)sender.tag);
}
- (void)togglePause:(id)sender {
    (void)sender;
    if (!self.loaded || self.failed) return;
    self.paused = !self.paused;
    [self updateControls];
}
- (void)restart:(id)sender {
    (void)sender;
    if (self.loaded && [self confirmDiscard]) [self loadROM:self.romURL eeprom:self.eepromURL];
}
- (BOOL)writeEEPROM {
    if (!self.loaded) return NO;
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = @"eeprom-session.bin";
    panel.message = @"Enregistrez une nouvelle EEPROM pour reprendre cette session plus tard.";
    if ([panel runModal] != NSModalResponseOK) return NO;
    for (NSURL *input in self.inputURLs) {
        if (sameFile(panel.URL, input)) {
            [self showError:@"Choisissez un autre fichier : les ROM et EEPROM chargées sont protégées contre l’écrasement."];
            return NO;
        }
    }
    NSError *error = nil;
    NSData *image = [self currentEEPROM];
    if (![image writeToURL:panel.URL options:NSDataWritingAtomic error:&error]) {
        [self showError:error.localizedDescription ?: @"Impossible d’exporter l’EEPROM."];
        return NO;
    }
    self.savedEEPROM = image;
    return YES;
}
- (void)exportEEPROM:(id)sender { (void)sender; [self writeEEPROM]; }
- (BOOL)validateMenuItem:(NSMenuItem *)item {
    if (item.action == @selector(exportEEPROM:) || item.action == @selector(restart:)) return self.loaded;
    if (item.action == @selector(togglePause:)) return self.loaded && !self.failed;
    return YES;
}
- (BOOL)windowShouldClose:(NSWindow *)sender { (void)sender; return [self confirmDiscard]; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    (void)sender;
    // Closing the window has already asked about the session.
    return !self.window.visible || [self confirmDiscard] ? NSTerminateNow : NSTerminateCancel;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { (void)sender; return YES; }
- (void)applicationWillTerminate:(NSNotification *)notification {
    (void)notification;
    [self.timer invalidate];
    if (self.eventMonitor) [NSEvent removeMonitor:self.eventMonitor];
}
- (void)about:(id)sender {
    (void)sender;
    [NSApp orderFrontStandardAboutPanelWithOptions:@{
        @"ApplicationName": @"PokeStroller",
        @"ApplicationVersion": @"0.2.0 — macOS",
        @"Copyright": @"Émulateur expérimental · jpcerrone et contributeurs · GPL-3.0"
    }];
}
@end

int main(int argc, const char *argv[]) {
    (void)argc; (void)argv;
    @autoreleasepool {
        NSApplication *app = NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        WalkerApp *delegate = [[WalkerApp alloc] init];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
