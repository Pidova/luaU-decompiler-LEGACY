#pragma once

/* 

False: 
	Gets args mostly through reg stack id (More accurate in some cases but can be unoptimized, depends who wrote the code)
		(Can follow other rules stated below for ambigous ones)

True:
	Gets args mostly through control flow (More optimized but can be inaccurate) following hueristics:
		* Must be used more then once as a source/reg following dest, each dest will reset it's count.
		* See control_flow_vars_ignore_useless if useless variables gets added.
		* Each var will respect seperate register (can be changed in the future depends on arch).
		* Used twice without being overwritten.
		* Gets used/written to inside a child scope but used before being written into its scope. (Child scopes make it more volatile)
		* Note if it abides none of this and insteads chucks itself in another expression(ie for loop) its not detected as a variable. 
		
*/
#define control_flow_vars true

#if control_flow_vars 
	#define control_flow_vars_ignore_useless false /* Ignores useless variables. (Variables that are created with dest but are never used in scope, if compiler didn't get rid of it before hand) */
#endif