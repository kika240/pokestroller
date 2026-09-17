#import <Cocoa/Cocoa.h>
#include <sys/stat.h>
#include "emulator.h"
#include "ir_link.h"
#include "macos_audio.h"
#import "session_store.h"

// All emulation and AppKit callbacks run on the main thread. No shared core state.
@interface WalkerDisplay : NSView
@property(nonatomic, strong) NSBitmapImageRep *bitmap;
@property(nonatomic) PWEmulator *emulator;
- (void)updateFrame;
@end

@implementation WalkerDisplay
- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame])) {
        _bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
            pixelsWide:PW_WIDTH pixelsHigh:PW_HEIGHT bitsPerSample:8 samplesPerPixel:3
            hasAlpha:NO isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
            bytesPerRow:PW_WIDTH * 3 bitsPerPixel:24];
        memset(_bitmap.bitmapData, 0xcc, PW_WIDTH * PW_HEIGHT * 3);
        self.accessibilityLabel = @"Écran du PokéWalker";
    }
    return self;
}
- (void)updateFrame {
    uint32_t pixels[PW_WIDTH * PW_HEIGHT];
    if (!self.emulator) return;
    pw_frame(self.emulator, pixels);
    unsigned char *out = self.bitmap.bitmapData;
    for (size_t i = 0; i < PW_WIDTH * PW_HEIGHT; i++) {
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
    CGFloat scale = MAX(1, floor(MIN((self.bounds.size.width - 24) / PW_WIDTH,
                                    (self.bounds.size.height - 24) / PW_HEIGHT)));
    NSRect screen = NSMakeRect(floor((self.bounds.size.width - PW_WIDTH * scale) / 2),
                               floor((self.bounds.size.height - PW_HEIGHT * scale) / 2),
                               PW_WIDTH * scale, PW_HEIGHT * scale);
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
@property(nonatomic, strong) NSButton *walkButton;
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
@property(nonatomic) PWEmulator *emulator;
@property(nonatomic) PWLink *link;
@property(nonatomic) PWAudio *audio;
@property(nonatomic, strong) PWSessionStore *store;
@property(nonatomic, strong) NSURL *saveRoot;
@property(nonatomic, copy) NSString *profile;
@property(nonatomic) BOOL walking;
@property(nonatomic) BOOL muted;
@property(nonatomic) float volume;
@property(nonatomic) uint8_t heldKeys;
@property(nonatomic) uint8_t pulseKeys;
@property(nonatomic) NSUInteger pulseTicks;
@property(nonatomic) NSUInteger ticks;
@property(nonatomic) int lastLinkState;
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
    [emulatorMenu addItem:[self item:@"Simuler la marche" action:@selector(toggleWalk:) key:@"m"]];
    [emulatorMenu addItem:[self item:@"Horloge du Mac…" action:@selector(clockInfo:) key:@""]];
    emulatorItem.submenu = emulatorMenu;
    NSMenuItem *soundItem = [bar addItemWithTitle:@"Son" action:NULL keyEquivalent:@""];
    NSMenu *soundMenu = [[NSMenu alloc] initWithTitle:@"Son"];
    [soundMenu addItem:[self item:@"Son activé" action:@selector(toggleSound:) key:@""]];
    [soundMenu addItem:[self item:@"Volume…" action:@selector(changeVolume:) key:@""]];
    soundItem.submenu = soundMenu;
    NSMenuItem *irItem = [bar addItemWithTitle:@"Infrarouge" action:NULL keyEquivalent:@""];
    NSMenu *irMenu = [[NSMenu alloc] initWithTitle:@"Infrarouge"];
    [irMenu addItem:[self item:@"Attendre une connexion locale…" action:@selector(listenIR:) key:@""]];
    [irMenu addItem:[self item:@"Se connecter…" action:@selector(connectIR:) key:@""]];
    [irMenu addItem:[self item:@"Déconnecter" action:@selector(disconnectIR:) key:@""]];
    irItem.submenu = irMenu;
    [fileMenu addItem:[self item:@"Dossier des sauvegardes" action:@selector(showSaves:) key:@""]];
    NSApp.mainMenu = bar;
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    self.profile = @"default";
    self.volume = 0.35;
    self.link = pw_link_create();
    self.audio = pw_audio_open();
    self.muted = self.audio == NULL;
    NSURL *support = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                       inDomains:NSUserDomainMask].firstObject;
    self.saveRoot = [support URLByAppendingPathComponent:@"PokeStroller/Sessions" isDirectory:YES];
    [self makeMenu];
    self.inputURLs = [NSMutableArray array];
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 510)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"PokeStroller";
    self.window.contentMinSize = NSMakeSize(580, 450);
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
    left.tag = PW_LEFT;
    NSButton *enter = [NSButton buttonWithTitle:@"●  Espace" target:self action:@selector(pressKey:)];
    enter.tag = PW_CENTER;
    NSButton *right = [NSButton buttonWithTitle:@"X / →  ▶" target:self action:@selector(pressKey:)];
    right.tag = PW_RIGHT;
    self.controls = @[left, enter, right];
    for (NSButton *button in self.controls) button.enabled = NO;
    self.pauseButton = [NSButton buttonWithTitle:@"Pause" target:self action:@selector(togglePause:)];
    self.pauseButton.enabled = NO;
    NSButton *walk = [NSButton buttonWithTitle:@"Marcher" target:self action:@selector(toggleWalk:)];
    walk.buttonType = NSButtonTypePushOnPushOff; self.walkButton=walk; walk.enabled=NO;
    NSStackView *buttons = [NSStackView stackViewWithViews:@[left, enter, right, self.pauseButton, walk]];
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
    self.eventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyDown | NSEventMaskKeyUp)
        handler:^NSEvent *(NSEvent *event) {
            WalkerApp *app = weakSelf;
            if (!app.loaded || app.paused || app.failed || event.window != app.window ||
                NSApp.modalWindow || (event.modifierFlags & (NSEventModifierFlagCommand |
                                                           NSEventModifierFlagControl |
                                                           NSEventModifierFlagOption))) return event;
            NSString *key = event.charactersIgnoringModifiers.lowercaseString;
            uint8_t input = 0;
            if ([key isEqualToString:@"z"] || event.keyCode == 123) input = PW_LEFT;
            else if ([key isEqualToString:@"x"] || event.keyCode == 124) input = PW_RIGHT;
            else if ([key isEqualToString:@" "] || event.keyCode == 36) input = PW_CENTER;
            if (!input) return event;
            if (!event.isARepeat) {
                BOOL down = event.type == NSEventTypeKeyDown;
                if (down) app.heldKeys |= input; else app.heldKeys &= ~input;
                pw_button(app.emulator, input, down);
            }
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
        else if ([args[i] isEqualToString:@"--profile"] && i + 1 < args.count) self.profile = args[++i];
        else if ([args[i] isEqualToString:@"--save-dir"] && i + 1 < args.count)
            self.saveRoot = [NSURL fileURLWithPath:args[++i] isDirectory:YES];
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
    NSMutableData *data = [NSMutableData dataWithLength:PW_EEPROM_SIZE];
    pw_eeprom(self.emulator, data.mutableBytes);
    return data;
}
- (BOOL)confirmDiscard {
    if (!self.loaded || [self saveSession]) return YES;
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Conserver les modifications de cette session ?";
    alert.informativeText = @"La sauvegarde automatique a échoué. Exportez la session pour conserver vos changements.";
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
    NSError *readError = nil;
    NSNumber *romSize = nil, *eeSize = nil;
    [rom getResourceValue:&romSize forKey:NSURLFileSizeKey error:&readError];
    [eeprom getResourceValue:&eeSize forKey:NSURLFileSizeKey error:&readError];
    if ((romSize.unsignedLongLongValue != PW_ROM_SIZE && romSize.unsignedLongLongValue != 65536) ||
        eeSize.unsignedLongLongValue != PW_EEPROM_SIZE) {
        [self showError:readError.localizedDescription ?: @"ROM : 48 ou 64 Kio ; EEPROM : exactement 64 Kio."]; return;
    }
    NSData *romData = [NSData dataWithContentsOfURL:rom options:0 error:&readError];
    NSData *eeData = [NSData dataWithContentsOfURL:eeprom options:0 error:&readError];
    if (!romData || !eeData) { [self showError:readError.localizedDescription]; return; }
    NSString *identifier = [PWSessionStore identifierForROM:romData eeprom:eeData profile:self.profile];
    PWSessionStore *store = [self.store.identifier isEqual:identifier] ? self.store :
        [[PWSessionStore alloc] initWithRoot:self.saveRoot identifier:identifier error:&readError];
    if (!store) { [self showError:readError.localizedDescription]; return; }
    NSData *restored = [store restore:&readError];
    if (readError) { [self showError:readError.localizedDescription]; return; }
    char error[2048];
    PWEmulator *emulator = pw_create(romData.bytes, romData.length,
        (restored ?: eeData).bytes, (restored ?: eeData).length, error, sizeof(error));
    if (!emulator) { [self showError:[NSString stringWithUTF8String:error]]; return; }
    pw_destroy(self.emulator);
    self.emulator = emulator;
    self.display.emulator = emulator;
    self.store = store;
    [self disconnectIR:nil];
    self.romURL = rom; self.eepromURL = eeprom;
    [self.inputURLs addObjectsFromArray:@[rom, eeprom]];
    self.cycles = self.ticks = self.heldKeys = self.pulseKeys = self.pulseTicks = 0;
    self.loaded = YES; self.failed = NO; self.paused = NO;
    pw_walk(emulator,self.walking);
    self.savedEEPROM = [self currentEEPROM];
    self.window.title = [NSString stringWithFormat:@"PokeStroller — %@ — %@", eeprom.lastPathComponent,self.profile];
    [self.display updateFrame]; [self updateControls];
    if (store.recoveredBackup) [self showError:@"La dernière sauvegarde était illisible. La copie précédente a été restaurée."];
}
- (BOOL)saveSession {
    if (!self.loaded) return YES;
    NSError *error = nil;
    NSData *data = [self currentEEPROM];
    if (![self.store save:data error:&error]) {
        self.status.stringValue = error.localizedDescription ?: @"Échec de la sauvegarde automatique.";
        return NO;
    }
    self.savedEEPROM = data;
    return YES;
}
- (void)updateControls {
    for (NSButton *button in self.controls) button.enabled = self.loaded && !self.paused && !self.failed;
    self.pauseButton.enabled = self.loaded && !self.failed;
    self.walkButton.enabled = self.loaded && !self.failed;
    self.walkButton.state = self.walking ? NSControlStateValueOn : NSControlStateValueOff;
    self.pauseButton.title = self.paused ? @"Reprendre" : @"Pause";
    pw_audio_active(self.audio,self.loaded && !self.paused && !self.failed && !self.muted);
    if (self.loaded && !self.failed) {
        NSString *ir = @[@"IR désactivé", @"IR en attente", @"IR connecté"][MAX(0,pw_link_state(self.link))];
        self.status.stringValue = [NSString stringWithFormat:@"%@ · %u pas · %u W · %@ · sauvegarde auto",
            self.paused ? @"En pause" : self.walking ? @"Marche simulée" : @"Au repos",
            pw_steps(self.emulator),pw_watts(self.emulator),ir];
        if (pw_link_state(self.link)<0) self.status.stringValue =
            [@"IR : " stringByAppendingString:[NSString stringWithUTF8String:pw_link_error(self.link)]];
    }
}
- (void)pumpIR {
    pw_link_poll(self.link);
    int state = pw_link_state(self.link);
    if (state != self.lastLinkState) { pw_ir_clear(self.emulator); self.lastLinkState = state; }
    uint8_t bytes[4096]; size_t count;
    while ((count = pw_ir_transmit(self.emulator,bytes,sizeof(bytes))))
        if (state == 2) pw_link_send(self.link,bytes,count);
    while ((count = pw_link_read(self.link,bytes,sizeof(bytes))))
        pw_ir_receive(self.emulator,bytes,count);
}
- (void)tick:(NSTimer *)timer {
    (void)timer;
    if (!self.loaded || self.paused || self.failed || NSApp.modalWindow) {
        pw_audio_active(self.audio,NO); return;
    }
    pw_audio_active(self.audio,!self.muted);
    // Service IR at 1/600 second boundaries; never catch up sleep/dialog time.
    for (int slice=0;slice<10;slice++) {
        [self pumpIR];
        if (!pw_run(self.emulator,PW_CLOCK/600)) {
            self.failed=YES; [self updateControls];
            self.status.stringValue=[NSString stringWithFormat:@"Arrêt à 0x%04X : %s",pw_pc(self.emulator),pw_error(self.emulator)];
            [self saveSession]; break;
        }
    }
    int16_t audio[2048]; size_t count;
    while ((count=pw_audio(self.emulator,audio,2048))) pw_audio_submit(self.audio,audio,count);
    if (self.pulseTicks && --self.pulseTicks==0) {
        pw_button(self.emulator,self.pulseKeys & ~self.heldKeys,false); self.pulseKeys=0;
    }
    self.ticks++;
    if (self.ticks % 4 == 0) [self.display updateFrame];
    if (self.ticks % 60 == 0) [self updateControls];
    if (self.ticks % 300 == 0 && ![self saveSession]) {
        self.paused=YES; [self updateControls];
        [self showError:@"Sauvegarde automatique impossible. La session est en pause ; exportez l’EEPROM avant de quitter."];
    }
}
- (void)pressKey:(NSButton *)sender {
    if (!self.loaded || self.paused || self.failed) return;
    self.pulseKeys |= (uint8_t)sender.tag; self.pulseTicks=sender.tag==PW_CENTER ? 60 : 6;
    pw_button(self.emulator,(uint8_t)sender.tag,true);
}
- (void)releaseKeys {
    pw_button(self.emulator,PW_LEFT|PW_CENTER|PW_RIGHT,false);
    self.heldKeys=self.pulseKeys=self.pulseTicks=0;
}
- (void)windowDidResignKey:(NSNotification *)note { (void)note; [self releaseKeys]; }
- (void)toggleWalk:(id)sender {
    if (!self.loaded || self.failed) return;
    self.walking=!self.walking; pw_walk(self.emulator,self.walking);
    if ([sender isKindOfClass:NSButton.class]) [sender setState:self.walking ? NSControlStateValueOn : NSControlStateValueOff];
    [self updateControls];
}
- (void)toggleSound:(id)sender { (void)sender; self.muted=!self.muted; [self updateControls]; }
- (void)changeVolume:(id)sender {
    (void)sender;
    NSAlert *alert=[[NSAlert alloc] init]; alert.messageText=@"Volume sonore";
    alert.informativeText=@"Le son doit aussi être activé dans les Réglages du PokéWalker. Le volume du Mac règle ensuite son amplification.";
    NSSlider *slider=[NSSlider sliderWithValue:self.volume minValue:0 maxValue:1 target:nil action:nil];
    slider.frame=NSMakeRect(0,0,260,30); alert.accessoryView=slider;
    [alert addButtonWithTitle:@"Appliquer"]; [alert addButtonWithTitle:@"Annuler"];
    if ([alert runModal]==NSAlertFirstButtonReturn) {self.volume=slider.floatValue;pw_audio_volume(self.audio,self.volume);}
}
- (void)clockInfo:(id)sender {
    (void)sender;
    NSAlert *alert=[[NSAlert alloc] init];alert.messageText=@"Horloge synchronisée avec le Mac";
    alert.informativeText=@"Le PokéWalker reçoit l’heure locale du Mac, avec les interruptions de seconde, minute et changement de jour. La marche simulée n’avance pas pendant une pause ou la veille du Mac.";
    [alert runModal];
}
- (NSString *)askValue:(NSString *)title initial:(NSString *)initial detail:(NSString *)detail {
    NSAlert *alert=[[NSAlert alloc] init];alert.messageText=title;alert.informativeText=detail;
    NSTextField *field=[[NSTextField alloc] initWithFrame:NSMakeRect(0,0,300,26)];field.stringValue=initial;
    alert.accessoryView=field;[alert addButtonWithTitle:@"Valider"];[alert addButtonWithTitle:@"Annuler"];
    return [alert runModal]==NSAlertFirstButtonReturn ? field.stringValue : nil;
}
- (void)listenIR:(id)sender {
    (void)sender;
    NSString *value=[self askValue:@"Port IR local" initial:@"31337"
        detail:@"Ouvrez une seconde instance avec un autre profil, connectez-la à ce port, puis choisissez Connexion sur les deux PokéWalker."];
    if (!value) return;
    NSInteger port=value.integerValue;
    if (port<1 || port>65535) { [self showError:@"Le port doit être compris entre 1 et 65535."];return; }
    pw_ir_clear(self.emulator);
    if (!pw_link_listen(self.link,(uint16_t)port)) [self showError:[NSString stringWithUTF8String:pw_link_error(self.link)]];
    [self updateControls];
}
- (void)connectIR:(id)sender {
    (void)sender;
    NSString *value=[self askValue:@"Correspondant IR" initial:@"127.0.0.1:31337"
        detail:@"Adresse IPv4:port d’une instance PokeStroller ou d’un serveur TCP IR compatible. Aucun adaptateur infrarouge physique n’est utilisé."];
    if (!value) return;
    NSArray *parts=[value componentsSeparatedByString:@":"];
    NSInteger port=parts.count==2 ? [parts[1] integerValue] : 0;
    if (parts.count!=2 || port<1 || port>65535) { [self showError:@"Format attendu : 127.0.0.1:31337"];return; }
    pw_ir_clear(self.emulator);
    if (!pw_link_connect(self.link,[parts[0] UTF8String],(uint16_t)port)) [self showError:[NSString stringWithUTF8String:pw_link_error(self.link)]];
    [self updateControls];
}
- (void)disconnectIR:(id)sender {
    (void)sender;pw_link_close(self.link);self.lastLinkState=0;
    if(self.emulator)pw_ir_clear(self.emulator);[self updateControls];
}
- (void)showSaves:(id)sender { (void)sender;if(self.store)[NSWorkspace.sharedWorkspace openURL:self.store.directory]; }
- (void)togglePause:(id)sender {
    (void)sender;
    if (!self.loaded || self.failed) return;
    self.paused = !self.paused;
    [self releaseKeys];
    if(self.paused && ![self saveSession]) [self showError:@"La sauvegarde automatique a échoué. Exportez la session avant de quitter."];
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
    if (item.action == @selector(toggleWalk:)) item.state=self.walking ? NSControlStateValueOn : NSControlStateValueOff;
    if (item.action == @selector(toggleSound:)) {item.state=self.muted ? NSControlStateValueOff : NSControlStateValueOn;return self.audio!=NULL;}
    if (item.action == @selector(togglePause:) || item.action == @selector(toggleWalk:) ||
        item.action == @selector(listenIR:) || item.action == @selector(connectIR:)) return self.loaded && !self.failed;
    if (item.action == @selector(showSaves:)) return self.store!=nil;
    if (item.action == @selector(disconnectIR:)) return pw_link_state(self.link)!=0;
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
    pw_audio_close(self.audio);pw_link_destroy(self.link);pw_destroy(self.emulator);
    if (self.eventMonitor) [NSEvent removeMonitor:self.eventMonitor];
}
- (void)about:(id)sender {
    (void)sender;
    [NSApp orderFrontStandardAboutPanelWithOptions:@{
        @"ApplicationName": @"PokeStroller",
        @"ApplicationVersion": @"0.3.0 — macOS",
        @"Copyright": @"Émulateur expérimental · jpcerrone, h4lfheart et contributeurs · GPL-3.0"
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
