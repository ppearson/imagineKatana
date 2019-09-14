/*
 ImagineKatana
 Copyright 2014-2019 Peter Pearson.

 Licensed under the Apache License, Version 2.0 (the "License");
 You may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ---------
*/

#ifndef IMAGINE_UTILS_H
#define IMAGINE_UTILS_H

// this type of thing isn't great, as we shouldn't have to do this (we should pull it from Imagine directly), but as
// we're working with Imagine's internals directly, it gets very messy...

// very nasty, but this is currently defined in internal Imagine files that shouldn't be exposed
enum BakedFlags
{
	GEO_QUANTISED					= 4,
	USE_INSTANCES					= 8,	// whether to allow instances to be treated as instances or not
	CONCAT_SUBOBJECT_TRANSFORMS		= 16,	// in Katana use-case, the render plugin itself is doing the transform concatenation, so Imagine can skip it
	HASH_TRANSFORM_STACK			= 32,
	CHUNKED_PARALLEL_BUILD			= (1 << 22)
};

#endif // IMAGINE_UTILS_H

