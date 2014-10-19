/*--------------------------------------------------------------------------*/
/*                                                                          */
/* Crowbar Code Refactoring Tool                                            */
/* author: Caian Benedicto                                                  */
/* contact: caianbene@gmail.com (with a [Crowbar] tag in the subject)       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#pragma once

extern const char* spyidt2;
#define optbase_0() { if(GenOpt) { cout << spyidt2 << endl; } }
#define message(x) cout << "Indeed " << x << endl;
#define error(x) cerr << "Indeed Error: " << x << endl;
#define assert_tool(x) {int err = (x); if(err) { cerr << "Indeed failed with error " \
<< err << endl; return err;} }
#define assert_phase(x) {int err = (x); if(err) return err; }
