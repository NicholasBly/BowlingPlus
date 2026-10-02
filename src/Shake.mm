#import <UIKit/UIKit.h>
#import <CoreMotion/CoreMotion.h>
#import "BFShared.h"
#include <math.h>

// Backup shake detector using the accelerometer (no permission prompt needed).
// A "shake" = 3 quick back-and-forth jolts within one second.
static CMMotionManager *sMotion;
static CFAbsoluteTime sLastShake;
static double sGx, sGy, sGz;
static bool sGInit;
static double sPeakTimes[8];
static int sPeakCount, sLastSign;

void BFHandleShake(NSString *source) {
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    if (now - sLastShake < 1.3) return;   // UIKit + CoreMotion can both report the same shake
    sLastShake = now;
    BFLog(@"shake (%@)", source);
    BFMenuToggle();
}

static void OnAccel(CMAcceleration a, NSTimeInterval t) {
    if (!sGInit) { sGx = a.x; sGy = a.y; sGz = a.z; sGInit = true; return; }
    // slow average = gravity; whatever is left is how hard the phone is being moved
    sGx = sGx * 0.9 + a.x * 0.1;
    sGy = sGy * 0.9 + a.y * 0.1;
    sGz = sGz * 0.9 + a.z * 0.1;
    double hx = a.x - sGx, hy = a.y - sGy, hz = a.z - sGz;
    if (sqrt(hx * hx + hy * hy + hz * hz) < 1.3) return;
    double ax = fabs(hx), ay = fabs(hy), az = fabs(hz);
    double dom = (ax >= ay && ax >= az) ? hx : (ay >= az ? hy : hz);
    int sign = dom > 0 ? 1 : -1;
    if (sign == sLastSign) return;        // needs back-and-forth, not one bump
    sLastSign = sign;
    int k = 0;
    for (int i = 0; i < sPeakCount; i++) if (t - sPeakTimes[i] < 1.0) sPeakTimes[k++] = sPeakTimes[i];
    sPeakCount = k;
    if (sPeakCount < 8) sPeakTimes[sPeakCount++] = t;
    if (sPeakCount >= 3) {
        sPeakCount = 0;
        sLastSign = 0;
        BFHandleShake(@"motion");
    }
}

void BFShakeStart(void) {
    if (sMotion) return;
    sMotion = [CMMotionManager new];
    if (!sMotion.accelerometerAvailable) { BFLog(@"accelerometer not available"); return; }
    sMotion.accelerometerUpdateInterval = 1.0 / 40.0;
    [sMotion startAccelerometerUpdatesToQueue:[NSOperationQueue mainQueue]
                                  withHandler:^(CMAccelerometerData *data, NSError *error) {
        if (data) OnAccel(data.acceleration, data.timestamp);
    }];
}
