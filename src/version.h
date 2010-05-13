#ifndef VERSION_H
#define VERSION_H

namespace AutoVersion{
	
	//Software Status
	static const char STATUS[] = "Beta";
	static const char STATUS_SHORT[] = "b";
	
	//Standard Version Type
	static const long MAJOR = 1;
	static const long MINOR = 2;
	static const long BUILD = 980;
	static const long REVISION = 5379;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT = 3635;
	#define RC_FILEVERSION 1,2,980,5379
	#define RC_FILEVERSION_STRING "1, 2, 980, 5379\0"
	static const char FULLVERSION_STRING[] = "1.2.980.5379";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY = 19;
	

}
#endif //VERSION_H
