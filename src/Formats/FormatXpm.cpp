/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatXpm.h"
#include "Common/BitmapDescription.h"
#include "Common/File.h"

#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>

namespace
{
    const size_t headerSize = 9;

    unsigned toRGBA(unsigned r, unsigned g, unsigned b, unsigned a)
    {
        return (a << 24) | (b << 16) | (g << 8) | (r);
    }

    unsigned toRGBA(unsigned r, unsigned g, unsigned b)
    {
        return toRGBA(r, g, b, 255);
    }

    struct sX11Colors
    {
        unsigned color;
        const char* name;
    };

    sX11Colors Colors[] = {
        // X11 colors
        { toRGBA(255, 250, 250), "snow" },
        { toRGBA(248, 248, 255), "ghost white" },
        { toRGBA(248, 248, 255), "GhostWhite" },
        { toRGBA(245, 245, 245), "white smoke" },
        { toRGBA(245, 245, 245), "WhiteSmoke" },
        { toRGBA(220, 220, 220), "gainsboro" },
        { toRGBA(255, 250, 240), "floral white" },
        { toRGBA(255, 250, 240), "FloralWhite" },
        { toRGBA(253, 245, 230), "old lace" },
        { toRGBA(253, 245, 230), "OldLace" },
        { toRGBA(250, 240, 230), "linen" },
        { toRGBA(250, 235, 215), "antique white" },
        { toRGBA(250, 235, 215), "AntiqueWhite" },
        { toRGBA(255, 239, 213), "papaya whip" },
        { toRGBA(255, 239, 213), "PapayaWhip" },
        { toRGBA(255, 235, 205), "blanched almond" },
        { toRGBA(255, 235, 205), "BlanchedAlmond" },
        { toRGBA(255, 228, 196), "bisque" },
        { toRGBA(255, 218, 185), "peach puff" },
        { toRGBA(255, 218, 185), "PeachPuff" },
        { toRGBA(255, 222, 173), "navajo white" },
        { toRGBA(255, 222, 173), "NavajoWhite" },
        { toRGBA(255, 228, 181), "moccasin" },
        { toRGBA(255, 248, 220), "cornsilk" },
        { toRGBA(255, 255, 240), "ivory" },
        { toRGBA(255, 250, 205), "lemon chiffon" },
        { toRGBA(255, 250, 205), "LemonChiffon" },
        { toRGBA(255, 245, 238), "seashell" },
        { toRGBA(240, 255, 240), "honeydew" },
        { toRGBA(245, 255, 250), "mint cream" },
        { toRGBA(245, 255, 250), "MintCream" },
        { toRGBA(240, 255, 255), "azure" },
        { toRGBA(240, 248, 255), "alice blue" },
        { toRGBA(240, 248, 255), "AliceBlue" },
        { toRGBA(230, 230, 250), "lavender" },
        { toRGBA(255, 240, 245), "lavender blush" },
        { toRGBA(255, 240, 245), "LavenderBlush" },
        { toRGBA(255, 228, 225), "misty rose" },
        { toRGBA(255, 228, 225), "MistyRose" },
        { toRGBA(255, 255, 255), "white" },
        { toRGBA(0, 0, 0), "black" },
        { toRGBA(47, 79, 79), "dark slate gray" },
        { toRGBA(47, 79, 79), "DarkSlateGray" },
        { toRGBA(47, 79, 79), "dark slate grey" },
        { toRGBA(47, 79, 79), "DarkSlateGrey" },
        { toRGBA(105, 105, 105), "dim gray" },
        { toRGBA(105, 105, 105), "DimGray" },
        { toRGBA(105, 105, 105), "dim grey" },
        { toRGBA(105, 105, 105), "DimGrey" },
        { toRGBA(112, 128, 144), "slate gray" },
        { toRGBA(112, 128, 144), "SlateGray" },
        { toRGBA(112, 128, 144), "slate grey" },
        { toRGBA(112, 128, 144), "SlateGrey" },
        { toRGBA(119, 136, 153), "light slate gray" },
        { toRGBA(119, 136, 153), "LightSlateGray" },
        { toRGBA(119, 136, 153), "light slate grey" },
        { toRGBA(119, 136, 153), "LightSlateGrey" },
        { toRGBA(190, 190, 190), "gray" },
        { toRGBA(190, 190, 190), "grey" },
        { toRGBA(190, 190, 190), "x11 gray" },
        { toRGBA(190, 190, 190), "X11Gray" },
        { toRGBA(190, 190, 190), "x11 grey" },
        { toRGBA(190, 190, 190), "X11Grey" },
        { toRGBA(128, 128, 128), "web gray" },
        { toRGBA(128, 128, 128), "WebGray" },
        { toRGBA(128, 128, 128), "web grey" },
        { toRGBA(128, 128, 128), "WebGrey" },
        { toRGBA(211, 211, 211), "light grey" },
        { toRGBA(211, 211, 211), "LightGrey" },
        { toRGBA(211, 211, 211), "light gray" },
        { toRGBA(211, 211, 211), "LightGray" },
        { toRGBA(25, 25, 112), "midnight blue" },
        { toRGBA(25, 25, 112), "MidnightBlue" },
        { toRGBA(0, 0, 128), "navy" },
        { toRGBA(0, 0, 128), "navy blue" },
        { toRGBA(0, 0, 128), "NavyBlue" },
        { toRGBA(100, 149, 237), "cornflower blue" },
        { toRGBA(100, 149, 237), "CornflowerBlue" },
        { toRGBA(72, 61, 139), "dark slate blue" },
        { toRGBA(72, 61, 139), "DarkSlateBlue" },
        { toRGBA(106, 90, 205), "slate blue" },
        { toRGBA(106, 90, 205), "SlateBlue" },
        { toRGBA(123, 104, 238), "medium slate blue" },
        { toRGBA(123, 104, 238), "MediumSlateBlue" },
        { toRGBA(132, 112, 255), "light slate blue" },
        { toRGBA(132, 112, 255), "LightSlateBlue" },
        { toRGBA(0, 0, 205), "medium blue" },
        { toRGBA(0, 0, 205), "MediumBlue" },
        { toRGBA(65, 105, 225), "royal blue" },
        { toRGBA(65, 105, 225), "RoyalBlue" },
        { toRGBA(0, 0, 255), "blue" },
        { toRGBA(30, 144, 255), "dodger blue" },
        { toRGBA(30, 144, 255), "DodgerBlue" },
        { toRGBA(0, 191, 255), "deep sky blue" },
        { toRGBA(0, 191, 255), "DeepSkyBlue" },
        { toRGBA(135, 206, 235), "sky blue" },
        { toRGBA(135, 206, 235), "SkyBlue" },
        { toRGBA(135, 206, 250), "light sky blue" },
        { toRGBA(135, 206, 250), "LightSkyBlue" },
        { toRGBA(70, 130, 180), "steel blue" },
        { toRGBA(70, 130, 180), "SteelBlue" },
        { toRGBA(176, 196, 222), "light steel blue" },
        { toRGBA(176, 196, 222), "LightSteelBlue" },
        { toRGBA(173, 216, 230), "light blue" },
        { toRGBA(173, 216, 230), "LightBlue" },
        { toRGBA(176, 224, 230), "powder blue" },
        { toRGBA(176, 224, 230), "PowderBlue" },
        { toRGBA(175, 238, 238), "pale turquoise" },
        { toRGBA(175, 238, 238), "PaleTurquoise" },
        { toRGBA(0, 206, 209), "dark turquoise" },
        { toRGBA(0, 206, 209), "DarkTurquoise" },
        { toRGBA(72, 209, 204), "medium turquoise" },
        { toRGBA(72, 209, 204), "MediumTurquoise" },
        { toRGBA(64, 224, 208), "turquoise" },
        { toRGBA(0, 255, 255), "cyan" },
        { toRGBA(0, 255, 255), "aqua" },
        { toRGBA(224, 255, 255), "light cyan" },
        { toRGBA(224, 255, 255), "LightCyan" },
        { toRGBA(95, 158, 160), "cadet blue" },
        { toRGBA(95, 158, 160), "CadetBlue" },
        { toRGBA(102, 205, 170), "medium aquamarine" },
        { toRGBA(102, 205, 170), "MediumAquamarine" },
        { toRGBA(127, 255, 212), "aquamarine" },
        { toRGBA(0, 100, 0), "dark green" },
        { toRGBA(0, 100, 0), "DarkGreen" },
        { toRGBA(85, 107, 47), "dark olive green" },
        { toRGBA(85, 107, 47), "DarkOliveGreen" },
        { toRGBA(143, 188, 143), "dark sea green" },
        { toRGBA(143, 188, 143), "DarkSeaGreen" },
        { toRGBA(46, 139, 87), "sea green" },
        { toRGBA(46, 139, 87), "SeaGreen" },
        { toRGBA(60, 179, 113), "medium sea green" },
        { toRGBA(60, 179, 113), "MediumSeaGreen" },
        { toRGBA(32, 178, 170), "light sea green" },
        { toRGBA(32, 178, 170), "LightSeaGreen" },
        { toRGBA(152, 251, 152), "pale green" },
        { toRGBA(152, 251, 152), "PaleGreen" },
        { toRGBA(0, 255, 127), "spring green" },
        { toRGBA(0, 255, 127), "SpringGreen" },
        { toRGBA(124, 252, 0), "lawn green" },
        { toRGBA(124, 252, 0), "LawnGreen" },
        { toRGBA(0, 255, 0), "green" },
        { toRGBA(0, 255, 0), "lime" },
        { toRGBA(0, 255, 0), "x11 green" },
        { toRGBA(0, 255, 0), "X11Green" },
        { toRGBA(0, 128, 0), "web green" },
        { toRGBA(0, 128, 0), "WebGreen" },
        { toRGBA(127, 255, 0), "chartreuse" },
        { toRGBA(0, 250, 154), "medium spring green" },
        { toRGBA(0, 250, 154), "MediumSpringGreen" },
        { toRGBA(173, 255, 47), "green yellow" },
        { toRGBA(173, 255, 47), "GreenYellow" },
        { toRGBA(50, 205, 50), "lime green" },
        { toRGBA(50, 205, 50), "LimeGreen" },
        { toRGBA(154, 205, 50), "yellow green" },
        { toRGBA(154, 205, 50), "YellowGreen" },
        { toRGBA(34, 139, 34), "forest green" },
        { toRGBA(34, 139, 34), "ForestGreen" },
        { toRGBA(107, 142, 35), "olive drab" },
        { toRGBA(107, 142, 35), "OliveDrab" },
        { toRGBA(189, 183, 107), "dark khaki" },
        { toRGBA(189, 183, 107), "DarkKhaki" },
        { toRGBA(240, 230, 140), "khaki" },
        { toRGBA(238, 232, 170), "pale goldenrod" },
        { toRGBA(238, 232, 170), "PaleGoldenrod" },
        { toRGBA(250, 250, 210), "light goldenrod yellow" },
        { toRGBA(250, 250, 210), "LightGoldenrodYellow" },
        { toRGBA(255, 255, 224), "light yellow" },
        { toRGBA(255, 255, 224), "LightYellow" },
        { toRGBA(255, 255, 0), "yellow" },
        { toRGBA(255, 215, 0), "gold" },
        { toRGBA(238, 221, 130), "light goldenrod" },
        { toRGBA(238, 221, 130), "LightGoldenrod" },
        { toRGBA(218, 165, 32), "goldenrod" },
        { toRGBA(184, 134, 11), "dark goldenrod" },
        { toRGBA(184, 134, 11), "DarkGoldenrod" },
        { toRGBA(188, 143, 143), "rosy brown" },
        { toRGBA(188, 143, 143), "RosyBrown" },
        { toRGBA(205, 92, 92), "indian red" },
        { toRGBA(205, 92, 92), "IndianRed" },
        { toRGBA(139, 69, 19), "saddle brown" },
        { toRGBA(139, 69, 19), "SaddleBrown" },
        { toRGBA(160, 82, 45), "sienna" },
        { toRGBA(205, 133, 63), "peru" },
        { toRGBA(222, 184, 135), "burlywood" },
        { toRGBA(245, 245, 220), "beige" },
        { toRGBA(245, 222, 179), "wheat" },
        { toRGBA(244, 164, 96), "sandy brown" },
        { toRGBA(244, 164, 96), "SandyBrown" },
        { toRGBA(210, 180, 140), "tan" },
        { toRGBA(210, 105, 30), "chocolate" },
        { toRGBA(178, 34, 34), "firebrick" },
        { toRGBA(165, 42, 42), "brown" },
        { toRGBA(233, 150, 122), "dark salmon" },
        { toRGBA(233, 150, 122), "DarkSalmon" },
        { toRGBA(250, 128, 114), "salmon" },
        { toRGBA(255, 160, 122), "light salmon" },
        { toRGBA(255, 160, 122), "LightSalmon" },
        { toRGBA(255, 165, 0), "orange" },
        { toRGBA(255, 140, 0), "dark orange" },
        { toRGBA(255, 140, 0), "DarkOrange" },
        { toRGBA(255, 127, 80), "coral" },
        { toRGBA(240, 128, 128), "light coral" },
        { toRGBA(240, 128, 128), "LightCoral" },
        { toRGBA(255, 99, 71), "tomato" },
        { toRGBA(255, 69, 0), "orange red" },
        { toRGBA(255, 69, 0), "OrangeRed" },
        { toRGBA(255, 0, 0), "red" },
        { toRGBA(255, 105, 180), "hot pink" },
        { toRGBA(255, 105, 180), "HotPink" },
        { toRGBA(255, 20, 147), "deep pink" },
        { toRGBA(255, 20, 147), "DeepPink" },
        { toRGBA(255, 192, 203), "pink" },
        { toRGBA(255, 182, 193), "light pink" },
        { toRGBA(255, 182, 193), "LightPink" },
        { toRGBA(219, 112, 147), "pale violet red" },
        { toRGBA(219, 112, 147), "PaleVioletRed" },
        { toRGBA(176, 48, 96), "maroon" },
        { toRGBA(176, 48, 96), "x11 maroon" },
        { toRGBA(176, 48, 96), "X11Maroon" },
        { toRGBA(128, 0, 0), "web maroon" },
        { toRGBA(128, 0, 0), "WebMaroon" },
        { toRGBA(199, 21, 133), "medium violet red" },
        { toRGBA(199, 21, 133), "MediumVioletRed" },
        { toRGBA(208, 32, 144), "violet red" },
        { toRGBA(208, 32, 144), "VioletRed" },
        { toRGBA(255, 0, 255), "magenta" },
        { toRGBA(255, 0, 255), "fuchsia" },
        { toRGBA(238, 130, 238), "violet" },
        { toRGBA(221, 160, 221), "plum" },
        { toRGBA(218, 112, 214), "orchid" },
        { toRGBA(186, 85, 211), "medium orchid" },
        { toRGBA(186, 85, 211), "MediumOrchid" },
        { toRGBA(153, 50, 204), "dark orchid" },
        { toRGBA(153, 50, 204), "DarkOrchid" },
        { toRGBA(148, 0, 211), "dark violet" },
        { toRGBA(148, 0, 211), "DarkViolet" },
        { toRGBA(138, 43, 226), "blue violet" },
        { toRGBA(138, 43, 226), "BlueViolet" },
        { toRGBA(160, 32, 240), "purple" },
        { toRGBA(160, 32, 240), "x11 purple" },
        { toRGBA(160, 32, 240), "X11Purple" },
        { toRGBA(128, 0, 128), "web purple" },
        { toRGBA(128, 0, 128), "WebPurple" },
        { toRGBA(147, 112, 219), "medium purple" },
        { toRGBA(147, 112, 219), "MediumPurple" },
        { toRGBA(216, 191, 216), "thistle" },
        { toRGBA(255, 250, 250), "snow1" },
        { toRGBA(238, 233, 233), "snow2" },
        { toRGBA(205, 201, 201), "snow3" },
        { toRGBA(139, 137, 137), "snow4" },
        { toRGBA(255, 245, 238), "seashell1" },
        { toRGBA(238, 229, 222), "seashell2" },
        { toRGBA(205, 197, 191), "seashell3" },
        { toRGBA(139, 134, 130), "seashell4" },
        { toRGBA(255, 239, 219), "AntiqueWhite1" },
        { toRGBA(238, 223, 204), "AntiqueWhite2" },
        { toRGBA(205, 192, 176), "AntiqueWhite3" },
        { toRGBA(139, 131, 120), "AntiqueWhite4" },
        { toRGBA(255, 228, 196), "bisque1" },
        { toRGBA(238, 213, 183), "bisque2" },
        { toRGBA(205, 183, 158), "bisque3" },
        { toRGBA(139, 125, 107), "bisque4" },
        { toRGBA(255, 218, 185), "PeachPuff1" },
        { toRGBA(238, 203, 173), "PeachPuff2" },
        { toRGBA(205, 175, 149), "PeachPuff3" },
        { toRGBA(139, 119, 101), "PeachPuff4" },
        { toRGBA(255, 222, 173), "NavajoWhite1" },
        { toRGBA(238, 207, 161), "NavajoWhite2" },
        { toRGBA(205, 179, 139), "NavajoWhite3" },
        { toRGBA(139, 121, 94), "NavajoWhite4" },
        { toRGBA(255, 250, 205), "LemonChiffon1" },
        { toRGBA(238, 233, 191), "LemonChiffon2" },
        { toRGBA(205, 201, 165), "LemonChiffon3" },
        { toRGBA(139, 137, 112), "LemonChiffon4" },
        { toRGBA(255, 248, 220), "cornsilk1" },
        { toRGBA(238, 232, 205), "cornsilk2" },
        { toRGBA(205, 200, 177), "cornsilk3" },
        { toRGBA(139, 136, 120), "cornsilk4" },
        { toRGBA(255, 255, 240), "ivory1" },
        { toRGBA(238, 238, 224), "ivory2" },
        { toRGBA(205, 205, 193), "ivory3" },
        { toRGBA(139, 139, 131), "ivory4" },
        { toRGBA(240, 255, 240), "honeydew1" },
        { toRGBA(224, 238, 224), "honeydew2" },
        { toRGBA(193, 205, 193), "honeydew3" },
        { toRGBA(131, 139, 131), "honeydew4" },
        { toRGBA(255, 240, 245), "LavenderBlush1" },
        { toRGBA(238, 224, 229), "LavenderBlush2" },
        { toRGBA(205, 193, 197), "LavenderBlush3" },
        { toRGBA(139, 131, 134), "LavenderBlush4" },
        { toRGBA(255, 228, 225), "MistyRose1" },
        { toRGBA(238, 213, 210), "MistyRose2" },
        { toRGBA(205, 183, 181), "MistyRose3" },
        { toRGBA(139, 125, 123), "MistyRose4" },
        { toRGBA(240, 255, 255), "azure1" },
        { toRGBA(224, 238, 238), "azure2" },
        { toRGBA(193, 205, 205), "azure3" },
        { toRGBA(131, 139, 139), "azure4" },
        { toRGBA(131, 111, 255), "SlateBlue1" },
        { toRGBA(122, 103, 238), "SlateBlue2" },
        { toRGBA(105, 89, 205), "SlateBlue3" },
        { toRGBA(71, 60, 139), "SlateBlue4" },
        { toRGBA(72, 118, 255), "RoyalBlue1" },
        { toRGBA(67, 110, 238), "RoyalBlue2" },
        { toRGBA(58, 95, 205), "RoyalBlue3" },
        { toRGBA(39, 64, 139), "RoyalBlue4" },
        { toRGBA(0, 0, 255), "blue1" },
        { toRGBA(0, 0, 238), "blue2" },
        { toRGBA(0, 0, 205), "blue3" },
        { toRGBA(0, 0, 139), "blue4" },
        { toRGBA(30, 144, 255), "DodgerBlue1" },
        { toRGBA(28, 134, 238), "DodgerBlue2" },
        { toRGBA(24, 116, 205), "DodgerBlue3" },
        { toRGBA(16, 78, 139), "DodgerBlue4" },
        { toRGBA(99, 184, 255), "SteelBlue1" },
        { toRGBA(92, 172, 238), "SteelBlue2" },
        { toRGBA(79, 148, 205), "SteelBlue3" },
        { toRGBA(54, 100, 139), "SteelBlue4" },
        { toRGBA(0, 191, 255), "DeepSkyBlue1" },
        { toRGBA(0, 178, 238), "DeepSkyBlue2" },
        { toRGBA(0, 154, 205), "DeepSkyBlue3" },
        { toRGBA(0, 104, 139), "DeepSkyBlue4" },
        { toRGBA(135, 206, 255), "SkyBlue1" },
        { toRGBA(126, 192, 238), "SkyBlue2" },
        { toRGBA(108, 166, 205), "SkyBlue3" },
        { toRGBA(74, 112, 139), "SkyBlue4" },
        { toRGBA(176, 226, 255), "LightSkyBlue1" },
        { toRGBA(164, 211, 238), "LightSkyBlue2" },
        { toRGBA(141, 182, 205), "LightSkyBlue3" },
        { toRGBA(96, 123, 139), "LightSkyBlue4" },
        { toRGBA(198, 226, 255), "SlateGray1" },
        { toRGBA(185, 211, 238), "SlateGray2" },
        { toRGBA(159, 182, 205), "SlateGray3" },
        { toRGBA(108, 123, 139), "SlateGray4" },
        { toRGBA(202, 225, 255), "LightSteelBlue1" },
        { toRGBA(188, 210, 238), "LightSteelBlue2" },
        { toRGBA(162, 181, 205), "LightSteelBlue3" },
        { toRGBA(110, 123, 139), "LightSteelBlue4" },
        { toRGBA(191, 239, 255), "LightBlue1" },
        { toRGBA(178, 223, 238), "LightBlue2" },
        { toRGBA(154, 192, 205), "LightBlue3" },
        { toRGBA(104, 131, 139), "LightBlue4" },
        { toRGBA(224, 255, 255), "LightCyan1" },
        { toRGBA(209, 238, 238), "LightCyan2" },
        { toRGBA(180, 205, 205), "LightCyan3" },
        { toRGBA(122, 139, 139), "LightCyan4" },
        { toRGBA(187, 255, 255), "PaleTurquoise1" },
        { toRGBA(174, 238, 238), "PaleTurquoise2" },
        { toRGBA(150, 205, 205), "PaleTurquoise3" },
        { toRGBA(102, 139, 139), "PaleTurquoise4" },
        { toRGBA(152, 245, 255), "CadetBlue1" },
        { toRGBA(142, 229, 238), "CadetBlue2" },
        { toRGBA(122, 197, 205), "CadetBlue3" },
        { toRGBA(83, 134, 139), "CadetBlue4" },
        { toRGBA(0, 245, 255), "turquoise1" },
        { toRGBA(0, 229, 238), "turquoise2" },
        { toRGBA(0, 197, 205), "turquoise3" },
        { toRGBA(0, 134, 139), "turquoise4" },
        { toRGBA(0, 255, 255), "cyan1" },
        { toRGBA(0, 238, 238), "cyan2" },
        { toRGBA(0, 205, 205), "cyan3" },
        { toRGBA(0, 139, 139), "cyan4" },
        { toRGBA(151, 255, 255), "DarkSlateGray1" },
        { toRGBA(141, 238, 238), "DarkSlateGray2" },
        { toRGBA(121, 205, 205), "DarkSlateGray3" },
        { toRGBA(82, 139, 139), "DarkSlateGray4" },
        { toRGBA(127, 255, 212), "aquamarine1" },
        { toRGBA(118, 238, 198), "aquamarine2" },
        { toRGBA(102, 205, 170), "aquamarine3" },
        { toRGBA(69, 139, 116), "aquamarine4" },
        { toRGBA(193, 255, 193), "DarkSeaGreen1" },
        { toRGBA(180, 238, 180), "DarkSeaGreen2" },
        { toRGBA(155, 205, 155), "DarkSeaGreen3" },
        { toRGBA(105, 139, 105), "DarkSeaGreen4" },
        { toRGBA(84, 255, 159), "SeaGreen1" },
        { toRGBA(78, 238, 148), "SeaGreen2" },
        { toRGBA(67, 205, 128), "SeaGreen3" },
        { toRGBA(46, 139, 87), "SeaGreen4" },
        { toRGBA(154, 255, 154), "PaleGreen1" },
        { toRGBA(144, 238, 144), "PaleGreen2" },
        { toRGBA(124, 205, 124), "PaleGreen3" },
        { toRGBA(84, 139, 84), "PaleGreen4" },
        { toRGBA(0, 255, 127), "SpringGreen1" },
        { toRGBA(0, 238, 118), "SpringGreen2" },
        { toRGBA(0, 205, 102), "SpringGreen3" },
        { toRGBA(0, 139, 69), "SpringGreen4" },
        { toRGBA(0, 255, 0), "green1" },
        { toRGBA(0, 238, 0), "green2" },
        { toRGBA(0, 205, 0), "green3" },
        { toRGBA(0, 139, 0), "green4" },
        { toRGBA(127, 255, 0), "chartreuse1" },
        { toRGBA(118, 238, 0), "chartreuse2" },
        { toRGBA(102, 205, 0), "chartreuse3" },
        { toRGBA(69, 139, 0), "chartreuse4" },
        { toRGBA(192, 255, 62), "OliveDrab1" },
        { toRGBA(179, 238, 58), "OliveDrab2" },
        { toRGBA(154, 205, 50), "OliveDrab3" },
        { toRGBA(105, 139, 34), "OliveDrab4" },
        { toRGBA(202, 255, 112), "DarkOliveGreen1" },
        { toRGBA(188, 238, 104), "DarkOliveGreen2" },
        { toRGBA(162, 205, 90), "DarkOliveGreen3" },
        { toRGBA(110, 139, 61), "DarkOliveGreen4" },
        { toRGBA(255, 246, 143), "khaki1" },
        { toRGBA(238, 230, 133), "khaki2" },
        { toRGBA(205, 198, 115), "khaki3" },
        { toRGBA(139, 134, 78), "khaki4" },
        { toRGBA(255, 236, 139), "LightGoldenrod1" },
        { toRGBA(238, 220, 130), "LightGoldenrod2" },
        { toRGBA(205, 190, 112), "LightGoldenrod3" },
        { toRGBA(139, 129, 76), "LightGoldenrod4" },
        { toRGBA(255, 255, 224), "LightYellow1" },
        { toRGBA(238, 238, 209), "LightYellow2" },
        { toRGBA(205, 205, 180), "LightYellow3" },
        { toRGBA(139, 139, 122), "LightYellow4" },
        { toRGBA(255, 255, 0), "yellow1" },
        { toRGBA(238, 238, 0), "yellow2" },
        { toRGBA(205, 205, 0), "yellow3" },
        { toRGBA(139, 139, 0), "yellow4" },
        { toRGBA(255, 215, 0), "gold1" },
        { toRGBA(238, 201, 0), "gold2" },
        { toRGBA(205, 173, 0), "gold3" },
        { toRGBA(139, 117, 0), "gold4" },
        { toRGBA(255, 193, 37), "goldenrod1" },
        { toRGBA(238, 180, 34), "goldenrod2" },
        { toRGBA(205, 155, 29), "goldenrod3" },
        { toRGBA(139, 105, 20), "goldenrod4" },
        { toRGBA(255, 185, 15), "DarkGoldenrod1" },
        { toRGBA(238, 173, 14), "DarkGoldenrod2" },
        { toRGBA(205, 149, 12), "DarkGoldenrod3" },
        { toRGBA(139, 101, 8), "DarkGoldenrod4" },
        { toRGBA(255, 193, 193), "RosyBrown1" },
        { toRGBA(238, 180, 180), "RosyBrown2" },
        { toRGBA(205, 155, 155), "RosyBrown3" },
        { toRGBA(139, 105, 105), "RosyBrown4" },
        { toRGBA(255, 106, 106), "IndianRed1" },
        { toRGBA(238, 99, 99), "IndianRed2" },
        { toRGBA(205, 85, 85), "IndianRed3" },
        { toRGBA(139, 58, 58), "IndianRed4" },
        { toRGBA(255, 130, 71), "sienna1" },
        { toRGBA(238, 121, 66), "sienna2" },
        { toRGBA(205, 104, 57), "sienna3" },
        { toRGBA(139, 71, 38), "sienna4" },
        { toRGBA(255, 211, 155), "burlywood1" },
        { toRGBA(238, 197, 145), "burlywood2" },
        { toRGBA(205, 170, 125), "burlywood3" },
        { toRGBA(139, 115, 85), "burlywood4" },
        { toRGBA(255, 231, 186), "wheat1" },
        { toRGBA(238, 216, 174), "wheat2" },
        { toRGBA(205, 186, 150), "wheat3" },
        { toRGBA(139, 126, 102), "wheat4" },
        { toRGBA(255, 165, 79), "tan1" },
        { toRGBA(238, 154, 73), "tan2" },
        { toRGBA(205, 133, 63), "tan3" },
        { toRGBA(139, 90, 43), "tan4" },
        { toRGBA(255, 127, 36), "chocolate1" },
        { toRGBA(238, 118, 33), "chocolate2" },
        { toRGBA(205, 102, 29), "chocolate3" },
        { toRGBA(139, 69, 19), "chocolate4" },
        { toRGBA(255, 48, 48), "firebrick1" },
        { toRGBA(238, 44, 44), "firebrick2" },
        { toRGBA(205, 38, 38), "firebrick3" },
        { toRGBA(139, 26, 26), "firebrick4" },
        { toRGBA(255, 64, 64), "brown1" },
        { toRGBA(238, 59, 59), "brown2" },
        { toRGBA(205, 51, 51), "brown3" },
        { toRGBA(139, 35, 35), "brown4" },
        { toRGBA(255, 140, 105), "salmon1" },
        { toRGBA(238, 130, 98), "salmon2" },
        { toRGBA(205, 112, 84), "salmon3" },
        { toRGBA(139, 76, 57), "salmon4" },
        { toRGBA(255, 160, 122), "LightSalmon1" },
        { toRGBA(238, 149, 114), "LightSalmon2" },
        { toRGBA(205, 129, 98), "LightSalmon3" },
        { toRGBA(139, 87, 66), "LightSalmon4" },
        { toRGBA(255, 165, 0), "orange1" },
        { toRGBA(238, 154, 0), "orange2" },
        { toRGBA(205, 133, 0), "orange3" },
        { toRGBA(139, 90, 0), "orange4" },
        { toRGBA(255, 127, 0), "DarkOrange1" },
        { toRGBA(238, 118, 0), "DarkOrange2" },
        { toRGBA(205, 102, 0), "DarkOrange3" },
        { toRGBA(139, 69, 0), "DarkOrange4" },
        { toRGBA(255, 114, 86), "coral1" },
        { toRGBA(238, 106, 80), "coral2" },
        { toRGBA(205, 91, 69), "coral3" },
        { toRGBA(139, 62, 47), "coral4" },
        { toRGBA(255, 99, 71), "tomato1" },
        { toRGBA(238, 92, 66), "tomato2" },
        { toRGBA(205, 79, 57), "tomato3" },
        { toRGBA(139, 54, 38), "tomato4" },
        { toRGBA(255, 69, 0), "OrangeRed1" },
        { toRGBA(238, 64, 0), "OrangeRed2" },
        { toRGBA(205, 55, 0), "OrangeRed3" },
        { toRGBA(139, 37, 0), "OrangeRed4" },
        { toRGBA(255, 0, 0), "red1" },
        { toRGBA(238, 0, 0), "red2" },
        { toRGBA(205, 0, 0), "red3" },
        { toRGBA(139, 0, 0), "red4" },
        { toRGBA(255, 20, 147), "DeepPink1" },
        { toRGBA(238, 18, 137), "DeepPink2" },
        { toRGBA(205, 16, 118), "DeepPink3" },
        { toRGBA(139, 10, 80), "DeepPink4" },
        { toRGBA(255, 110, 180), "HotPink1" },
        { toRGBA(238, 106, 167), "HotPink2" },
        { toRGBA(205, 96, 144), "HotPink3" },
        { toRGBA(139, 58, 98), "HotPink4" },
        { toRGBA(255, 181, 197), "pink1" },
        { toRGBA(238, 169, 184), "pink2" },
        { toRGBA(205, 145, 158), "pink3" },
        { toRGBA(139, 99, 108), "pink4" },
        { toRGBA(255, 174, 185), "LightPink1" },
        { toRGBA(238, 162, 173), "LightPink2" },
        { toRGBA(205, 140, 149), "LightPink3" },
        { toRGBA(139, 95, 101), "LightPink4" },
        { toRGBA(255, 130, 171), "PaleVioletRed1" },
        { toRGBA(238, 121, 159), "PaleVioletRed2" },
        { toRGBA(205, 104, 137), "PaleVioletRed3" },
        { toRGBA(139, 71, 93), "PaleVioletRed4" },
        { toRGBA(255, 52, 179), "maroon1" },
        { toRGBA(238, 48, 167), "maroon2" },
        { toRGBA(205, 41, 144), "maroon3" },
        { toRGBA(139, 28, 98), "maroon4" },
        { toRGBA(255, 62, 150), "VioletRed1" },
        { toRGBA(238, 58, 140), "VioletRed2" },
        { toRGBA(205, 50, 120), "VioletRed3" },
        { toRGBA(139, 34, 82), "VioletRed4" },
        { toRGBA(255, 0, 255), "magenta1" },
        { toRGBA(238, 0, 238), "magenta2" },
        { toRGBA(205, 0, 205), "magenta3" },
        { toRGBA(139, 0, 139), "magenta4" },
        { toRGBA(255, 131, 250), "orchid1" },
        { toRGBA(238, 122, 233), "orchid2" },
        { toRGBA(205, 105, 201), "orchid3" },
        { toRGBA(139, 71, 137), "orchid4" },
        { toRGBA(255, 187, 255), "plum1" },
        { toRGBA(238, 174, 238), "plum2" },
        { toRGBA(205, 150, 205), "plum3" },
        { toRGBA(139, 102, 139), "plum4" },
        { toRGBA(224, 102, 255), "MediumOrchid1" },
        { toRGBA(209, 95, 238), "MediumOrchid2" },
        { toRGBA(180, 82, 205), "MediumOrchid3" },
        { toRGBA(122, 55, 139), "MediumOrchid4" },
        { toRGBA(191, 62, 255), "DarkOrchid1" },
        { toRGBA(178, 58, 238), "DarkOrchid2" },
        { toRGBA(154, 50, 205), "DarkOrchid3" },
        { toRGBA(104, 34, 139), "DarkOrchid4" },
        { toRGBA(155, 48, 255), "purple1" },
        { toRGBA(145, 44, 238), "purple2" },
        { toRGBA(125, 38, 205), "purple3" },
        { toRGBA(85, 26, 139), "purple4" },
        { toRGBA(171, 130, 255), "MediumPurple1" },
        { toRGBA(159, 121, 238), "MediumPurple2" },
        { toRGBA(137, 104, 205), "MediumPurple3" },
        { toRGBA(93, 71, 139), "MediumPurple4" },
        { toRGBA(255, 225, 255), "thistle1" },
        { toRGBA(238, 210, 238), "thistle2" },
        { toRGBA(205, 181, 205), "thistle3" },
        { toRGBA(139, 123, 139), "thistle4" },
        { toRGBA(0, 0, 0), "gray0" },
        { toRGBA(0, 0, 0), "grey0" },
        { toRGBA(3, 3, 3), "gray1" },
        { toRGBA(3, 3, 3), "grey1" },
        { toRGBA(5, 5, 5), "gray2" },
        { toRGBA(5, 5, 5), "grey2" },
        { toRGBA(8, 8, 8), "gray3" },
        { toRGBA(8, 8, 8), "grey3" },
        { toRGBA(10, 10, 10), "gray4" },
        { toRGBA(10, 10, 10), "grey4" },
        { toRGBA(13, 13, 13), "gray5" },
        { toRGBA(13, 13, 13), "grey5" },
        { toRGBA(15, 15, 15), "gray6" },
        { toRGBA(15, 15, 15), "grey6" },
        { toRGBA(18, 18, 18), "gray7" },
        { toRGBA(18, 18, 18), "grey7" },
        { toRGBA(20, 20, 20), "gray8" },
        { toRGBA(20, 20, 20), "grey8" },
        { toRGBA(23, 23, 23), "gray9" },
        { toRGBA(23, 23, 23), "grey9" },
        { toRGBA(26, 26, 26), "gray10" },
        { toRGBA(26, 26, 26), "grey10" },
        { toRGBA(28, 28, 28), "gray11" },
        { toRGBA(28, 28, 28), "grey11" },
        { toRGBA(31, 31, 31), "gray12" },
        { toRGBA(31, 31, 31), "grey12" },
        { toRGBA(33, 33, 33), "gray13" },
        { toRGBA(33, 33, 33), "grey13" },
        { toRGBA(36, 36, 36), "gray14" },
        { toRGBA(36, 36, 36), "grey14" },
        { toRGBA(38, 38, 38), "gray15" },
        { toRGBA(38, 38, 38), "grey15" },
        { toRGBA(41, 41, 41), "gray16" },
        { toRGBA(41, 41, 41), "grey16" },
        { toRGBA(43, 43, 43), "gray17" },
        { toRGBA(43, 43, 43), "grey17" },
        { toRGBA(46, 46, 46), "gray18" },
        { toRGBA(46, 46, 46), "grey18" },
        { toRGBA(48, 48, 48), "gray19" },
        { toRGBA(48, 48, 48), "grey19" },
        { toRGBA(51, 51, 51), "gray20" },
        { toRGBA(51, 51, 51), "grey20" },
        { toRGBA(54, 54, 54), "gray21" },
        { toRGBA(54, 54, 54), "grey21" },
        { toRGBA(56, 56, 56), "gray22" },
        { toRGBA(56, 56, 56), "grey22" },
        { toRGBA(59, 59, 59), "gray23" },
        { toRGBA(59, 59, 59), "grey23" },
        { toRGBA(61, 61, 61), "gray24" },
        { toRGBA(61, 61, 61), "grey24" },
        { toRGBA(64, 64, 64), "gray25" },
        { toRGBA(64, 64, 64), "grey25" },
        { toRGBA(66, 66, 66), "gray26" },
        { toRGBA(66, 66, 66), "grey26" },
        { toRGBA(69, 69, 69), "gray27" },
        { toRGBA(69, 69, 69), "grey27" },
        { toRGBA(71, 71, 71), "gray28" },
        { toRGBA(71, 71, 71), "grey28" },
        { toRGBA(74, 74, 74), "gray29" },
        { toRGBA(74, 74, 74), "grey29" },
        { toRGBA(77, 77, 77), "gray30" },
        { toRGBA(77, 77, 77), "grey30" },
        { toRGBA(79, 79, 79), "gray31" },
        { toRGBA(79, 79, 79), "grey31" },
        { toRGBA(82, 82, 82), "gray32" },
        { toRGBA(82, 82, 82), "grey32" },
        { toRGBA(84, 84, 84), "gray33" },
        { toRGBA(84, 84, 84), "grey33" },
        { toRGBA(87, 87, 87), "gray34" },
        { toRGBA(87, 87, 87), "grey34" },
        { toRGBA(89, 89, 89), "gray35" },
        { toRGBA(89, 89, 89), "grey35" },
        { toRGBA(92, 92, 92), "gray36" },
        { toRGBA(92, 92, 92), "grey36" },
        { toRGBA(94, 94, 94), "gray37" },
        { toRGBA(94, 94, 94), "grey37" },
        { toRGBA(97, 97, 97), "gray38" },
        { toRGBA(97, 97, 97), "grey38" },
        { toRGBA(99, 99, 99), "gray39" },
        { toRGBA(99, 99, 99), "grey39" },
        { toRGBA(102, 102, 102), "gray40" },
        { toRGBA(102, 102, 102), "grey40" },
        { toRGBA(105, 105, 105), "gray41" },
        { toRGBA(105, 105, 105), "grey41" },
        { toRGBA(107, 107, 107), "gray42" },
        { toRGBA(107, 107, 107), "grey42" },
        { toRGBA(110, 110, 110), "gray43" },
        { toRGBA(110, 110, 110), "grey43" },
        { toRGBA(112, 112, 112), "gray44" },
        { toRGBA(112, 112, 112), "grey44" },
        { toRGBA(115, 115, 115), "gray45" },
        { toRGBA(115, 115, 115), "grey45" },
        { toRGBA(117, 117, 117), "gray46" },
        { toRGBA(117, 117, 117), "grey46" },
        { toRGBA(120, 120, 120), "gray47" },
        { toRGBA(120, 120, 120), "grey47" },
        { toRGBA(122, 122, 122), "gray48" },
        { toRGBA(122, 122, 122), "grey48" },
        { toRGBA(125, 125, 125), "gray49" },
        { toRGBA(125, 125, 125), "grey49" },
        { toRGBA(127, 127, 127), "gray50" },
        { toRGBA(127, 127, 127), "grey50" },
        { toRGBA(130, 130, 130), "gray51" },
        { toRGBA(130, 130, 130), "grey51" },
        { toRGBA(133, 133, 133), "gray52" },
        { toRGBA(133, 133, 133), "grey52" },
        { toRGBA(135, 135, 135), "gray53" },
        { toRGBA(135, 135, 135), "grey53" },
        { toRGBA(138, 138, 138), "gray54" },
        { toRGBA(138, 138, 138), "grey54" },
        { toRGBA(140, 140, 140), "gray55" },
        { toRGBA(140, 140, 140), "grey55" },
        { toRGBA(143, 143, 143), "gray56" },
        { toRGBA(143, 143, 143), "grey56" },
        { toRGBA(145, 145, 145), "gray57" },
        { toRGBA(145, 145, 145), "grey57" },
        { toRGBA(148, 148, 148), "gray58" },
        { toRGBA(148, 148, 148), "grey58" },
        { toRGBA(150, 150, 150), "gray59" },
        { toRGBA(150, 150, 150), "grey59" },
        { toRGBA(153, 153, 153), "gray60" },
        { toRGBA(153, 153, 153), "grey60" },
        { toRGBA(156, 156, 156), "gray61" },
        { toRGBA(156, 156, 156), "grey61" },
        { toRGBA(158, 158, 158), "gray62" },
        { toRGBA(158, 158, 158), "grey62" },
        { toRGBA(161, 161, 161), "gray63" },
        { toRGBA(161, 161, 161), "grey63" },
        { toRGBA(163, 163, 163), "gray64" },
        { toRGBA(163, 163, 163), "grey64" },
        { toRGBA(166, 166, 166), "gray65" },
        { toRGBA(166, 166, 166), "grey65" },
        { toRGBA(168, 168, 168), "gray66" },
        { toRGBA(168, 168, 168), "grey66" },
        { toRGBA(171, 171, 171), "gray67" },
        { toRGBA(171, 171, 171), "grey67" },
        { toRGBA(173, 173, 173), "gray68" },
        { toRGBA(173, 173, 173), "grey68" },
        { toRGBA(176, 176, 176), "gray69" },
        { toRGBA(176, 176, 176), "grey69" },
        { toRGBA(179, 179, 179), "gray70" },
        { toRGBA(179, 179, 179), "grey70" },
        { toRGBA(181, 181, 181), "gray71" },
        { toRGBA(181, 181, 181), "grey71" },
        { toRGBA(184, 184, 184), "gray72" },
        { toRGBA(184, 184, 184), "grey72" },
        { toRGBA(186, 186, 186), "gray73" },
        { toRGBA(186, 186, 186), "grey73" },
        { toRGBA(189, 189, 189), "gray74" },
        { toRGBA(189, 189, 189), "grey74" },
        { toRGBA(191, 191, 191), "gray75" },
        { toRGBA(191, 191, 191), "grey75" },
        { toRGBA(194, 194, 194), "gray76" },
        { toRGBA(194, 194, 194), "grey76" },
        { toRGBA(196, 196, 196), "gray77" },
        { toRGBA(196, 196, 196), "grey77" },
        { toRGBA(199, 199, 199), "gray78" },
        { toRGBA(199, 199, 199), "grey78" },
        { toRGBA(201, 201, 201), "gray79" },
        { toRGBA(201, 201, 201), "grey79" },
        { toRGBA(204, 204, 204), "gray80" },
        { toRGBA(204, 204, 204), "grey80" },
        { toRGBA(207, 207, 207), "gray81" },
        { toRGBA(207, 207, 207), "grey81" },
        { toRGBA(209, 209, 209), "gray82" },
        { toRGBA(209, 209, 209), "grey82" },
        { toRGBA(212, 212, 212), "gray83" },
        { toRGBA(212, 212, 212), "grey83" },
        { toRGBA(214, 214, 214), "gray84" },
        { toRGBA(214, 214, 214), "grey84" },
        { toRGBA(217, 217, 217), "gray85" },
        { toRGBA(217, 217, 217), "grey85" },
        { toRGBA(219, 219, 219), "gray86" },
        { toRGBA(219, 219, 219), "grey86" },
        { toRGBA(222, 222, 222), "gray87" },
        { toRGBA(222, 222, 222), "grey87" },
        { toRGBA(224, 224, 224), "gray88" },
        { toRGBA(224, 224, 224), "grey88" },
        { toRGBA(227, 227, 227), "gray89" },
        { toRGBA(227, 227, 227), "grey89" },
        { toRGBA(229, 229, 229), "gray90" },
        { toRGBA(229, 229, 229), "grey90" },
        { toRGBA(232, 232, 232), "gray91" },
        { toRGBA(232, 232, 232), "grey91" },
        { toRGBA(235, 235, 235), "gray92" },
        { toRGBA(235, 235, 235), "grey92" },
        { toRGBA(237, 237, 237), "gray93" },
        { toRGBA(237, 237, 237), "grey93" },
        { toRGBA(240, 240, 240), "gray94" },
        { toRGBA(240, 240, 240), "grey94" },
        { toRGBA(242, 242, 242), "gray95" },
        { toRGBA(242, 242, 242), "grey95" },
        { toRGBA(245, 245, 245), "gray96" },
        { toRGBA(245, 245, 245), "grey96" },
        { toRGBA(247, 247, 247), "gray97" },
        { toRGBA(247, 247, 247), "grey97" },
        { toRGBA(250, 250, 250), "gray98" },
        { toRGBA(250, 250, 250), "grey98" },
        { toRGBA(252, 252, 252), "gray99" },
        { toRGBA(252, 252, 252), "grey99" },
        { toRGBA(255, 255, 255), "gray100" },
        { toRGBA(255, 255, 255), "grey100" },
        { toRGBA(169, 169, 169), "dark grey" },
        { toRGBA(169, 169, 169), "DarkGrey" },
        { toRGBA(169, 169, 169), "dark gray" },
        { toRGBA(169, 169, 169), "DarkGray" },
        { toRGBA(0, 0, 139), "dark blue" },
        { toRGBA(0, 0, 139), "DarkBlue" },
        { toRGBA(0, 139, 139), "dark cyan" },
        { toRGBA(0, 139, 139), "DarkCyan" },
        { toRGBA(139, 0, 139), "dark magenta" },
        { toRGBA(139, 0, 139), "DarkMagenta" },
        { toRGBA(139, 0, 0), "dark red" },
        { toRGBA(139, 0, 0), "DarkRed" },
        { toRGBA(144, 238, 144), "light green" },
        { toRGBA(144, 238, 144), "LightGreen" },
        { toRGBA(220, 20, 60), "crimson" },
        { toRGBA(75, 0, 130), "indigo" },
        { toRGBA(128, 128, 0), "olive" },
        { toRGBA(102, 51, 153), "rebecca purple" },
        { toRGBA(102, 51, 153), "RebeccaPurple" },
        { toRGBA(192, 192, 192), "silver" },
        { toRGBA(0, 128, 128), "teal" },

        // custom colors
        { toRGBA(0, 0, 0), "opaque" },
    };

    bool isValidFormat(const void* data, unsigned size)
    {
        return size >= headerSize && memcmp("/* XPM */", data, headerSize) == 0;
    }

    void replaceTabs(char* buffer)
    {
        for (size_t i = 0, size = ::strlen(buffer); i < size; i++)
        {
            if (buffer[i] == '\t')
            {
                buffer[i] = ' ';
            }
        }
    }

    unsigned parseColor(const char* color)
    {
        if (color[0] == '#')
        {
            color++;

            char val[32];

            const unsigned size = ::strlen(color) / 3;
            if (size < std::size(val))
            {
                unsigned r, g, b;
                unsigned i;

                for (i = 0; i < size; i++)
                {
                    val[i] = color[i + (0 * size)];
                }
                val[i] = 0;
                ::sscanf(val, "%x", &r);

                for (i = 0; i < size; i++)
                {
                    val[i] = color[i + (1 * size)];
                }
                val[i] = 0;
                ::sscanf(val, "%x", &g);

                for (i = 0; i < size; i++)
                {
                    val[i] = color[i + (2 * size)];
                }
                val[i] = 0;
                ::sscanf(val, "%x", &b);

                if (size == 1)
                {
                    r = (r << 4) | r;
                    g = (g << 4) | g;
                    b = (b << 4) | b;
                }
                else if (size > 2)
                {
                    r >>= (size - 2) * 4;
                    g >>= (size - 2) * 4;
                    b >>= (size - 2) * 4;
                }

                return toRGBA(r, g, b);
            }

            ::printf("(WW) Unknown color #: '%s'\n", color);
        }
        else if (::strncasecmp(color, "none", 4) != 0)
        {
            for (const auto& c : Colors)
            {
                if (::strncasecmp(c.name, color, ::strlen(c.name)) == 0)
                {
                    return c.color;
                }
            }

            auto fallBack = ::strstr(color, " c ");
            if (fallBack != nullptr)
            {
                return parseColor(fallBack + 3);
            }

            ::printf("(WW) Unknown color name: '%s'\n", color);
        }

        return 0;
    }

    typedef std::map<std::string, unsigned> ColorMap;

    unsigned getColor(const ColorMap& colorMap, const char* pixel)
    {
        auto it = colorMap.find(pixel);
        if (it != colorMap.end())
        {
            return it->second;
        }

        return toRGBA(0xff, 0, 0);
    }

    const char* getNextLine(const char* data)
    {
        // skip line
        for (; *data != 0 && *data != '\n'; data++)
        {
        }

        bool comment = false;
        for (; *data != 0; data++)
        {
            if (comment == false)
            {
                if (*data == '"')
                {
                    break;
                }

                if (data[0] == '/' && data[1] == '*')
                {
                    comment = true;
                }
            }
            else
            {
                if (data[0] == '*' && data[1] == '/')
                {
                    comment = false;
                }
            }
        }

        return data + 1;
    }
} // namespace

bool cFormatXpm::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, headerSize))
    {
        return false;
    }

    auto data = static_cast<const void*>(buffer.data());
    return isValidFormat(data, buffer.size());
}

bool cFormatXpm::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!openFile(file, filename, desc))
    {
        return false;
    }

    auto size = file.getSize();

    std::vector<char> buffer(size + 1);
    auto data = buffer.data();

    if (file.read(data, size) != size)
    {
        ::printf("(EE) Can't read XPM data.\n");
        return false;
    }

    data[size] = 0;

    if (isValidFormat(data, buffer.size()) == false)
    {
        ::printf("(EE) Invalid XPM header.\n");
        return false;
    }

    ColorMap colorMap;

    const char* line = data;

    // read image header
    line = getNextLine(line);
    unsigned width;
    unsigned height;
    unsigned colorsCount;
    unsigned cpp;
    if (::sscanf(line, "%u %u %u %u", &width, &height, &colorsCount, &cpp) != 4)
    {
        ::printf("(EE) Invalid XPM header.\n");
        return false;
    }

    // read color table
    for (unsigned i = 0; i < colorsCount; i++)
    {
        line = getNextLine(line);
        char pixel[100];
        ::memcpy(pixel, line, cpp);
        pixel[cpp] = 0;

        char color[100];
        char type;
        if (::sscanf(line + cpp, "\t%c %99[^\"]", &type, color) != 2)
        {
            ::printf("(EE) Can't read colors.\n");
            return false;
        }

        replaceTabs(color);
        colorMap[pixel] = parseColor(color);
    }

    desc.width = width;
    desc.height = height;
    desc.allocate(width, height, 32, ePixelFormat::RGBA);

    desc.bppImage = 24;

    // fill bitmap
    auto out = reinterpret_cast<unsigned*>(desc.bitmap.data());
    for (unsigned y = 0; y < height; y++)
    {
        line = getNextLine(line);
        for (unsigned x = 0; x < width; x++)
        {
            char pixel[100];
            memcpy(pixel, line, cpp);
            pixel[cpp] = 0;

            *out = getColor(colorMap, pixel);

            line += cpp;
            out++;
        }
    }

    desc.formatName = "xpm";

    return true;
}
