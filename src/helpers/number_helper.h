/*
 * *****************************************************************************
 *   Copyright 2014-2015 Spectra Logic Corporation. All Rights Reserved.
 *   Licensed under the Apache License, Version 2.0 (the "License"). You may not
 *   use this file except in compliance with the License. A copy of the License
 *   is located at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   or in the "license" file accompanying this file.
 *   This file is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *   CONDITIONS OF ANY KIND, either express or implied. See the License for the
 *   specific language governing permissions and limitations under the License.
 * *****************************************************************************
 */

#ifndef NUMBER_HELPER_H
#define NUMBER_HELPER_H

#include <stdint.h>
#include <QString>

class NumberHelper
{
public:
	static const uint64_t B;
	static const uint64_t KB;
	static const uint64_t MB;
	static const uint64_t GB;
	static const uint64_t TB;

	static QString ToHumanSize(uint64_t bytes);
	static QString ToHumanRate(uint64_t bytes);
};

inline QString
NumberHelper::ToHumanRate(uint64_t bytes)
{
	return (ToHumanSize(bytes) + "/S");
}

#endif
