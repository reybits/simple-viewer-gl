/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

// https://github.com/glfw/glfw/issues/1024#issuecomment-522667555

#pragma once

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface GLFWCustomDelegate : NSObject
+ (void)load; // load is called before even main() is run (as part of objc class registration)
@end

NS_ASSUME_NONNULL_END
