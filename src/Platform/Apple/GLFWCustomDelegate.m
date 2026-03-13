/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

// https://github.com/glfw/glfw/issues/1024#issuecomment-522667555

// See GLFWCustomDelegate.h for why this guard exists.
#ifdef __clang__

#import "GLFWCustomDelegate.h"
#import <objc/runtime.h>

extern void AddFile(const char* filename);

@implementation GLFWCustomDelegate

+ (void)load
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^
    {
        Class class = objc_getClass("GLFWApplicationDelegate");

        [GLFWCustomDelegate swizzle: class src: @selector(application: openFile:) tgt: @selector(swz_application: openFile:)];
        [GLFWCustomDelegate swizzle: class src: @selector(application: openFiles:) tgt: @selector(swz_application: openFiles:)];
    });
}

+ (void) swizzle: (Class) original_c src: (SEL)original_s tgt: (SEL)target_s
{
    Class target_c = [GLFWCustomDelegate class];
    Method originalMethod = class_getInstanceMethod(original_c, original_s);
    Method swizzledMethod = class_getInstanceMethod(target_c, target_s);

    BOOL didAddMethod =
        class_addMethod(original_c,
                        original_s,
                        method_getImplementation(swizzledMethod),
                        method_getTypeEncoding(swizzledMethod));

    if (didAddMethod)
    {
        class_replaceMethod(original_c,
                            target_s,
                            method_getImplementation(originalMethod),
                            method_getTypeEncoding(originalMethod));
    }
    else
    {
        method_exchangeImplementations(originalMethod, swizzledMethod);
    }
}

- (BOOL)swz_application: (NSApplication*)sender openFile: (NSString*)filename
{
    AddFile(filename.UTF8String);
    return true;
}

- (void)swz_application: (NSApplication*)sender openFiles: (NSArray<NSString*>*)filenames
{
    for (NSString* filename in filenames)
    {
        AddFile(filename.UTF8String);
    }
}

@end

#endif
