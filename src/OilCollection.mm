#import <Foundation/Foundation.h>
#import "BFShared.h"

// The BowlingPlus collection: real Kegel patterns typed in from their data sheets, ready to play.
//
// "drop" is the sheet's Reverse Brush Drop (feet): the engine only lays reverse oil short of it.
// Each step is { start board, stop board, loads, speed (in/s), travel-to feet }, exactly as the sheet's
// Forward / Reverse tables list them. Boards are 1-39 from the left (2L = 2, 2R = 38, 13R = 27).
// "Travel to" is only used by zero-load steps (the sheet's "End" column for those rows).
// The game's own Kegel engine turns these into oil, the same way it draws its built-in patterns.
//
// Checked against each sheet before adding: boards crossed (forward / reverse), total volume
// (boards crossed x oil per board), and every step's end distance.

static NSArray *B(int a, int b, int loads, int speed, double ft) { return @[ @(a), @(b), @(loads), @(speed), @(ft) ]; }

NSArray<NSDictionary *> *BFOilCollection(void) {
    static NSArray *all;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        all = @[
            // 2012 USBC Open Championships, Baton Rouge. Sheet: 39 ft, 50 uL/board, 25.2 mL,
            // 385 forward / 119 reverse boards crossed, reverse brush drop 34 ft.
            @{ @"id": @"bp.batonrouge2012", @"collection": @YES,
               @"name": @"Baton Rouge", @"event": @"2012 USBC Open Championships",
               @"feet": @39, @"ml": @25.2, @"ul": @50, @"base": @0, @"drop": @34, @"exact": @YES, @"precise": @YES,
               @"fwd": @[ B(2, 38, 6, 14, 9.9),  B(5, 35, 1, 18, 12.4), B(6, 34, 1, 18, 14.9), B(7, 33, 1, 18, 17.4),
                          B(9, 31, 1, 18, 19.9), B(10, 30, 1, 18, 22.4), B(12, 28, 1, 18, 24.9), B(13, 27, 1, 18, 27.4),
                          B(2, 38, 0, 22, 35.0), B(2, 38, 0, 26, 39.0) ],
               @"rev": @[ B(2, 38, 0, 30, 20.0), B(13, 27, 1, 22, 16.9), B(11, 29, 1, 18, 14.4), B(8, 32, 1, 18, 11.9),
                          B(6, 34, 1, 18, 9.4),  B(5, 35, 1, 18, 6.9),  B(2, 38, 0, 10, 0.0) ] },

            // 2026 PBA Regional 37 (Kegel). From the official .Pattern file. Sheet: 37 ft, 50 uL/board,
            // 32.65 mL (18.65 forward / 14 reverse), 373 / 280 boards crossed, drop brush 30 ft.
            // Lopsided (2L-6R, 4L-9R, 11L-10R, 13L-12R): needs Kegel sides, which BowlingPlus draws.
            @{ @"id": @"bp.pbaregional37.2026", @"collection": @YES,
               @"name": @"PBA Regional 37", @"event": @"2026 PBA Regional",
               @"feet": @37, @"ml": @32.65, @"ul": @50, @"base": @0, @"drop": @30, @"exact": @YES, @"precise": @YES,
               @"fwd": @[ B(2, 38, 3, 14, 3.92), B(2, 34, 1, 14, 5.88), B(7, 33, 3, 14, 11.76), B(4, 31, 2, 14, 15.68),
                          B(11, 30, 3, 18, 23.24), B(13, 28, 2, 18, 28.28), B(2, 38, 0, 22, 37.0) ],
               @"rev": @[ B(2, 38, 0, 30, 29.0), B(11, 29, 2, 22, 22.84), B(9, 32, 2, 18, 17.8), B(7, 33, 2, 18, 12.76),
                          B(6, 34, 1, 14, 10.8), B(2, 38, 3, 14, 4.92), B(2, 38, 0, 14, 0.0) ] },
        ];
    });
    return all;
}
