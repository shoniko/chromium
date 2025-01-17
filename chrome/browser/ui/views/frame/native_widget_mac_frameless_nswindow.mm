// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/native_widget_mac_frameless_nswindow.h"

@interface NSWindow (PrivateAPI)
+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle;
@end

@interface NativeWidgetMacFramelessNSWindowFrame
    : NativeWidgetMacNSWindowTitledFrame
@end

@implementation NativeWidgetMacFramelessNSWindowFrame
- (BOOL)_hidingTitlebar {
  return YES;
}
@end

@implementation NativeWidgetMacFramelessNSWindow

+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle {
  if ([NativeWidgetMacFramelessNSWindowFrame class]) {
    return [NativeWidgetMacFramelessNSWindowFrame class];
  }
  return [super frameViewClassForStyleMask:windowStyle];
}

@end
