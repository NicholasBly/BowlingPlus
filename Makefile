TARGET := iphone:clang:latest:14.0
ARCHS := arm64
INSTALL_TARGET_PROCESSES = BowlingbyJasonBelmonte

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = BowlingPlus

BowlingPlus_FILES = Tweak.xm $(wildcard src/*.mm)
BowlingPlus_CFLAGS = -fobjc-arc -Isrc -Wno-unused-function -Wno-unused-variable -Wno-deprecated-declarations
BowlingPlus_CCFLAGS = -std=c++17
BowlingPlus_FRAMEWORKS = UIKit Foundation QuartzCore CoreMotion CoreGraphics Network AVFoundation PhotosUI CoreImage UniformTypeIdentifiers PDFKit

# Sideloaded (non-jailbroken) apps cannot patch code at runtime, so this tweak never
# uses Substrate/ElleKit. The "internal" Logos generator only swizzles ObjC methods.
BowlingPlus_LOGOS_DEFAULT_GENERATOR = internal

include $(THEOS_MAKE_PATH)/tweak.mk
