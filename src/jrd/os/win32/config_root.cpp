/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		config_root.cpp
 *	DESCRIPTION:	Configuration manager (platform specific - Win32)
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * Created by: Dmitry Yemanov <yemanov@yandex.ru>
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#include <windows.h>

#include "fb_types.h"
#include "fb_string.h"

#include "../jrd/os/config_root.h"
#include "../jrd/os/path_utils.h"

typedef Firebird::string string;

const string CONFIG_FILE = "firebird.conf";

/******************************************************************************
 *
 *	Platform-specific root locator
 */

ConfigRoot::ConfigRoot()
{
	TEXT buffer[MAXPATHLEN];

	// get the pathname of the running executable
	GetModuleFileName(NULL, buffer, sizeof(buffer));
	string bin_dir = buffer;
	
	// get rid of the filename
	int index = bin_dir.rfind(PathUtils::dir_sep);
	bin_dir = bin_dir.substr(0, index);

	// how should we decide to use bin_dir instead of root_dir? any ideas?
	// ???

	// go to the parent directory
	index = bin_dir.rfind(PathUtils::dir_sep, bin_dir.length());
	root_dir = (index ? bin_dir.substr(0, index) : bin_dir) + PathUtils::dir_sep;
}

string ConfigRoot::getRootDirectory() const
{
	return root_dir;
}

string ConfigRoot::getConfigFile() const
{
	return root_dir + CONFIG_FILE;
}
