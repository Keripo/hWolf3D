#ifndef __VERSION_H__
#define __VERSION_H__

/* WL1 = 0 */ 
/* WL6 = 1 */
/* SDM = 2 */
/* SOD = 3 */
#ifndef WMODE
#define WMODE 1
#endif

#if WMODE == 0
/* #define SPEAR */
/* #define SPEARDEMO */
/* #define JAPAN */
/* #define JAPDEMO */
#define GOODTIMES
#define UPLOAD

#elif WMODE == 1
/* #define SPEAR */
/* #define SPEARDEMO */
/* #define JAPAN */
/* #define JAPDEMO */
#define GOODTIMES
/* #define UPLOAD */

#elif WMODE == 2
#define SPEAR 
#define SPEARDEMO 
/* #define JAPAN */
/* #define JAPDEMO */
#define GOODTIMES
/* #define UPLOAD */

#elif WMODE == 3
#define SPEAR
/* #define SPEARDEMO */
/* #define JAPAN */
/* #define JAPDEMO */
#define GOODTIMES
/* #define UPLOAD */

/*
JAPANDEMO requires UPLOAD
SPANISH
*/

#else
#error "please edit version.h and fix WMODE"
#endif

#else
#error "fix me: TODO"
#endif
