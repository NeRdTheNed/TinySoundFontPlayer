
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vTinySoundFontPlayer
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vTinySoundFontPlayer
#import <TinySoundFontPlayerAU/IPlugAUViewController.h>
#import <TinySoundFontPlayerAU/IPlugAUAudioUnit.h>

//! Project version number for TinySoundFontPlayerAU.
FOUNDATION_EXPORT double TinySoundFontPlayerAUVersionNumber;

//! Project version string for TinySoundFontPlayerAU.
FOUNDATION_EXPORT const unsigned char TinySoundFontPlayerAUVersionString[];

@class IPlugAUViewController_vTinySoundFontPlayer;
