#include "shell.h"

#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include <GLFW/glfw3.h>

#include "overlay_shape.h"

namespace {

std::function<void(mac::MenuCommand)> gOnCommand;
std::function<void()> gOnReopen;
NSStatusItem *gStatusItem = nil;
NSMenuItem *gToggleItem = nil;
NSMenuItem *gOverlayItem = nil;
bool gTrayTracking = false;
CFAbsoluteTime gPendingWake = INFINITY;

void command(mac::MenuCommand c) {
    if (gOnCommand) {
        gOnCommand(c);
    }
    mac::wake();
}

// 44×36 px template image, shown at 18 pt high.
NSImage *trayIcon(bool tracking) {
    constexpr int W = 44;
    constexpr int H = 36;
    constexpr int kSamples = 4;
    constexpr double cx = 22.0, cy = 18.0, hw = 18.0, hh = 12.0, radius = 5.0, stroke = 3.0;
    constexpr double dotRadius = 5.0;

    const auto inRoundedRect = [](double x, double y, double w, double h, double r) {
        const double dx = std::max(std::abs(x - cx) - (w - r), 0.0);
        const double dy = std::max(std::abs(y - cy) - (h - r), 0.0);
        return dx * dx + dy * dy <= r * r;
    };
    const auto covered = [&](double x, double y) {
        const bool outline =
          inRoundedRect(x, y, hw, hh, radius) && !inRoundedRect(x, y, hw - stroke, hh - stroke, radius - stroke);
        const bool dot = tracking && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= dotRadius * dotRadius;
        return outline || dot;
    };

    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                                    pixelsWide:W
                                                                    pixelsHigh:H
                                                                 bitsPerSample:8
                                                               samplesPerPixel:4
                                                                      hasAlpha:YES
                                                                      isPlanar:NO
                                                                colorSpaceName:NSDeviceRGBColorSpace
                                                                   bytesPerRow:W * 4
                                                                  bitsPerPixel:32];
    unsigned char *rgba = rep.bitmapData;
    for (int py = 0; py < H; py++) {
        for (int px = 0; px < W; px++) {
            int hits = 0;
            for (int sy = 0; sy < kSamples; sy++) {
                for (int sx = 0; sx < kSamples; sx++) {
                    hits += covered(px + (sx + 0.5) / kSamples, py + (sy + 0.5) / kSamples);
                }
            }
            unsigned char *pixel = rgba + (py * W + px) * 4;
            pixel[0] = pixel[1] = pixel[2] = 0;
            pixel[3] = static_cast<unsigned char>(hits * 255 / (kSamples * kSamples));
        }
    }
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(W / 2.0, H / 2.0)];
    [image addRepresentation:rep];
    [image setTemplate:YES];
    return image;
}

}  // namespace

@interface MenuCommandTarget : NSObject
@end

@implementation MenuCommandTarget
- (void)toggleTracking:(id)sender {
    command(mac::MenuCommand::toggleTracking);
}
- (void)showSettings:(id)sender {
    command(mac::MenuCommand::showSettings);
}
- (void)toggleOverlay:(id)sender {
    command(mac::MenuCommand::toggleOverlay);
}
- (void)hideSettings:(id)sender {
    command(mac::MenuCommand::hideSettings);
}
- (void)quit:(id)sender {
    command(mac::MenuCommand::quit);
}
@end

namespace {

MenuCommandTarget *gTarget = nil;

NSMenuItem *item(NSString *title, SEL action, NSString *key = @"") {
    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key];
    menuItem.target = gTarget;
    return menuItem;
}

void installMainMenu() {
    NSMenu *main = [[NSMenu alloc] init];
    NSMenuItem *appItem = [[NSMenuItem alloc] init];
    [main addItem:appItem];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"macOSu-pad"];
    [appMenu addItem:item(@"Close Window", @selector(hideSettings:), @"w")];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItem:item(@"Quit macOSu-pad", @selector(quit:), @"q")];
    appItem.submenu = appMenu;
    NSApp.mainMenu = main;
}

void installStatusItem() {
    gStatusItem = [NSStatusBar.systemStatusBar statusItemWithLength:NSVariableStatusItemLength];
    gStatusItem.button.image = trayIcon(false);
    gStatusItem.button.toolTip = @"macOSu-pad";

    NSMenu *menu = [[NSMenu alloc] init];
    menu.autoenablesItems = NO;
    NSMenuItem *version = [[NSMenuItem alloc] initWithTitle:@"macOSu-pad " MACOSU_PAD_VERSION action:nil keyEquivalent:@""];
    version.enabled = NO;
    [menu addItem:version];
    [menu addItem:[NSMenuItem separatorItem]];
    gToggleItem = item(@"Start tracking", @selector(toggleTracking:));
    [menu addItem:gToggleItem];
    [menu addItem:item(@"Settings…", @selector(showSettings:))];
    gOverlayItem = item(@"Finger overlay", @selector(toggleOverlay:));
    [menu addItem:gOverlayItem];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:item(@"Quit macOSu-pad", @selector(quit:))];
    gStatusItem.menu = menu;
}

// GLFW owns the application delegate, so the reopen handler is added to its class.
void installReopenHandler() {
    id delegate = NSApp.delegate;
    if (!delegate) {
        return;
    }
    const SEL selector = @selector(applicationShouldHandleReopen:hasVisibleWindows:);
    IMP handler = imp_implementationWithBlock(^BOOL(id, NSApplication *, BOOL) {
      if (gOnReopen) {
          gOnReopen();
      }
      mac::wake();
      return YES;
    });
    class_replaceMethod([delegate class], selector, handler, "c@:@c");
}

}  // namespace

void mac::prepareGlfw() {
    glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);
}

void mac::install(std::function<void(MenuCommand)> onCommand, std::function<void()> onReopen) {
    gOnCommand = std::move(onCommand);
    gOnReopen = std::move(onReopen);
    gTarget = [[MenuCommandTarget alloc] init];
    installMainMenu();
    installStatusItem();
    installReopenHandler();
}

void mac::setTrayState(bool available, bool tracking, bool overlay) {
    gToggleItem.enabled = available;
    gToggleItem.title = tracking ? @"Stop tracking" : @"Start tracking";
    gOverlayItem.state = overlay ? NSControlStateValueOn : NSControlStateValueOff;
    if (tracking != gTrayTracking || !gStatusItem.button.image) {
        gTrayTracking = tracking;
        gStatusItem.button.image = trayIcon(tracking);
    }
}

void mac::setDockIconVisible(bool visible) {
    [NSApp setActivationPolicy:visible ? NSApplicationActivationPolicyRegular : NSApplicationActivationPolicyAccessory];
    if (visible) {
        [NSApp activate];
    }
}

void mac::wake() {
    @autoreleasepool {
        NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:0
                                             context:nil
                                             subtype:0
                                               data1:0
                                               data2:0];
        [NSApp postEvent:event atStart:YES];
    }
}

void mac::wakeAfter(double seconds) {
    const CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + std::max(0.0, seconds);
    if (deadline >= gPendingWake - 0.001) {
        return;
    }
    gPendingWake = deadline;
    dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(std::max(0.0, seconds) * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        if (gPendingWake == deadline) {
            gPendingWake = INFINITY;
        }
        mac::wake();
      });
}

void mac::setDarkAppearance(bool dark) {
    NSApp.appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

@interface FingerOverlayView : NSView {
@public
    std::optional<macosu_pad::OverlayShape> shape;
}
@end

@implementation FingerOverlayView

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)isOpaque {
    return NO;
}

- (NSView *)hitTest:(NSPoint)point {
    return nil;
}

static void strokeTwice(CGContextRef ctx, CGPathRef path, CGFloat width) {
    CGContextAddPath(ctx, path);
    CGContextSetLineWidth(ctx, width + 2.0);
    CGContextSetRGBStrokeColor(ctx, 0.0, 0.0, 0.0, 0.55);
    CGContextStrokePath(ctx);
    CGContextAddPath(ctx, path);
    CGContextSetLineWidth(ctx, width);
    CGContextSetRGBStrokeColor(ctx, 1.0, 1.0, 1.0, 0.95);
    CGContextStrokePath(ctx);
}

- (void)drawRect:(NSRect)dirtyRect {
    CGContextRef ctx = NSGraphicsContext.currentContext.CGContext;
    CGContextClearRect(ctx, dirtyRect);
    if (!shape) {
        return;
    }
    CGContextSetLineJoin(ctx, kCGLineJoinRound);
    CGContextSetLineCap(ctx, kCGLineCapRound);

    CGMutablePathRef ellipse = CGPathCreateMutable();
    for (size_t i = 0; i < shape->ellipse.size(); i++) {
        const auto &p = shape->ellipse[i];
        if (i == 0) {
            CGPathMoveToPoint(ellipse, nullptr, p.x, p.y);
        } else {
            CGPathAddLineToPoint(ellipse, nullptr, p.x, p.y);
        }
    }
    CGPathCloseSubpath(ellipse);
    strokeTwice(ctx, ellipse, 2.0);
    CGPathRelease(ellipse);

    CGMutablePathRef dot = CGPathCreateMutable();
    CGPathAddEllipseInRect(
      dot, nullptr,
      CGRectMake(
        shape->tipEdge.x - macosu_pad::kOverlayDotRadius, shape->tipEdge.y - macosu_pad::kOverlayDotRadius,
        2 * macosu_pad::kOverlayDotRadius, 2 * macosu_pad::kOverlayDotRadius));
    strokeTwice(ctx, dot, 1.5);
    CGPathRelease(dot);

    CGMutablePathRef cross = CGPathCreateMutable();
    const double h = macosu_pad::kOverlayCrossHalf;
    CGPathMoveToPoint(cross, nullptr, shape->aim.x - h, shape->aim.y);
    CGPathAddLineToPoint(cross, nullptr, shape->aim.x + h, shape->aim.y);
    CGPathMoveToPoint(cross, nullptr, shape->aim.x, shape->aim.y - h);
    CGPathAddLineToPoint(cross, nullptr, shape->aim.x, shape->aim.y + h);
    strokeTwice(ctx, cross, 1.0);
    CGPathRelease(cross);
}

@end

struct mac::FingerOverlay::Impl {
    NSPanel *panel = nil;
    FingerOverlayView *view = nil;
    macosu_pad::Config config;
    std::optional<NSRect> drawn;
};

mac::FingerOverlay::FingerOverlay() : _impl(std::make_unique<Impl>()) {
    // Primary display: CoreGraphics global coordinates are relative to it.
    NSScreen *screen = NSScreen.screens.firstObject ?: NSScreen.mainScreen;
    const NSRect frame = screen ? screen.frame : NSMakeRect(0, 0, 1, 1);
    NSPanel *panel = [[NSPanel alloc] initWithContentRect:frame
                                                styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    panel.releasedWhenClosed = NO;
    panel.opaque = NO;
    panel.backgroundColor = NSColor.clearColor;
    panel.hasShadow = NO;
    panel.ignoresMouseEvents = YES;
    panel.hidesOnDeactivate = NO;
    panel.floatingPanel = YES;
    panel.level = NSScreenSaverWindowLevel;
    panel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary |
                               NSWindowCollectionBehaviorStationary | NSWindowCollectionBehaviorIgnoresCycle;
    FingerOverlayView *view =
      [[FingerOverlayView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height)];
    panel.contentView = view;
    [panel setFrame:frame display:NO];
    _impl->panel = panel;
    _impl->view = view;
}

mac::FingerOverlay::~FingerOverlay() {
    [_impl->panel orderOut:nil];
}

void mac::FingerOverlay::setVisible(bool visible) {
    if (visible) {
        [_impl->panel orderFrontRegardless];
    } else {
        [_impl->panel orderOut:nil];
    }
}

void mac::FingerOverlay::setConfig(const macosu_pad::Config &config) {
    _impl->config = config;
}

void mac::FingerOverlay::update(const macosu_pad::MonitorFrame &frame) {
    auto &impl = *_impl;
    auto shape = macosu_pad::overlayShape(impl.config, frame.geometry, frame.cursor);
    if (!shape && !impl.drawn) {
        return;
    }
    constexpr double kMargin = 4.0;
    std::optional<NSRect> next;
    if (shape) {
        next = NSMakeRect(
          shape->min.x - kMargin, shape->min.y - kMargin, shape->max.x - shape->min.x + 2 * kMargin,
          shape->max.y - shape->min.y + 2 * kMargin);
    }
    if (impl.drawn) {
        [impl.view setNeedsDisplayInRect:*impl.drawn];
    }
    if (next) {
        [impl.view setNeedsDisplayInRect:*next];
    }
    impl.view->shape = std::move(shape);
    impl.drawn = next;
}
