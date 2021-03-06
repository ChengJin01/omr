/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "MacroTool.hpp"

#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>

#include "NamespaceUDT.hpp"

using std::unordered_map;

string
MacroTool::getTypeName(string s)
{
	string typeEyeCatcher = "@TYPE_";
	return s.substr(typeEyeCatcher.size());
}

pair<string, string>
MacroTool::getMacroInfo(string s)
{
	string macroEyeCatcher = "@MACRO_";
	size_t pos = s.find(" ");
	string name = s.substr(macroEyeCatcher.size(), pos - macroEyeCatcher.size());
	/* pos is the location of a space. Add +1 to remove the space from the substring */
	string value = s.substr(pos + 1);
	pair<string, string> p(name, value);
	return p;
}

string
MacroTool::getFileName(string s)
{
	string fileNameEyeCatcher = "@DDRFILE_BEGIN ";
	string fileName = s.substr(fileNameEyeCatcher.size());
	return fileName;
}

/**
 * This function reads in a formatted file (fname) full of macro names, their
 * values and their associated types. This information is used to fill in a
 * vector of structure's (MacroInfo) amalgamating macros with their associated
 * type name. This vector is returned as an output parameter.
 * @return DDR_RC_OK on failure, DDR_RC_ERROR if an error is encountered.
 */
DDR_RC
MacroTool::getMacros(string fname)
{
	DDR_RC rc = DDR_RC_OK;

	if (fname.size() > 0) {
		string line;
		ifstream infile(fname.c_str());
		set<string> includeFileSet;

		/* Parse the input file for macros with values. */
		while (getline(infile, line)) {
			if (0 == line.find("@DDRFILE_BEGIN")) {
				string fileName = getFileName(line);
				if (includeFileSet.end() != includeFileSet.find(fileName)) {
					/* This include file was already processed.
					 * Scan until the file end delimiter is found.
					 */
					while (getline(infile, line)) {
						if (0 == line.find("@DDRFILE_END")) {
							break;
						}
					}
				} else {
					includeFileSet.insert(fileName);
				}
			} else if (string::npos != line.find("@TYPE_")) {
				MacroInfo m(getTypeName(line));
				macroList.push_back(m);
			} else if (string::npos != line.find("@MACRO_")) {
				pair<string, string> macro = getMacroInfo(line);
				if (!macro.second.empty()) {
					macroList.back().addMacro(macro);
				}
			}
		}
	} else {
		rc = DDR_RC_ERROR;
		ERRMSG("invalid fname");
	}

	return rc;
}

DDR_RC
MacroTool::addMacrosToIR(Symbol_IR *ir)
{
	DDR_RC rc = DDR_RC_OK;

	/* Create a map of name to IR Type for all types in the IR which
	 * can contain macros. This is used to add macros to existing types.
	 */
	unordered_map<string, Type *> irMap;
	for (vector<Type *>::iterator it = ir->_types.begin(); it != ir->_types.end(); it += 1) {
		NamespaceUDT *ns = dynamic_cast<NamespaceUDT *>(*it);
		if (NULL != ns) {
			irMap[(*it)->_name] = *it;
		}
	}

	for (vector<MacroInfo>::iterator it = macroList.begin(); it != macroList.end(); it += 1) {
		/* For each found MacroInfo which contains macros, add the macros to the IR. */
		MacroInfo *macroInfo = &*it;
		if (macroInfo->getNumMacros() > 0) {
			NamespaceUDT *ns = NULL;
			/* If there is a type of the correct name already, use it. Otherwise,
			 * create a new namespace to contain the macros.
			 */
			if (irMap.find(macroInfo->getTypeName()) == irMap.end()) {
				ns = new NamespaceUDT();
				ns->_name = macroInfo->getTypeName();
				ir->_types.push_back(ns);
				irMap[macroInfo->getTypeName()] = ns;
			} else {
				ns = dynamic_cast<NamespaceUDT *>(irMap[macroInfo->getTypeName()]);
			}

			if (NULL == ns) {
				ERRMSG("Cannot find or create UDT to add macro");
				rc = DDR_RC_ERROR;
				break;
			} else {
				for (set<pair<string, string>>::iterator it = macroInfo->getMacroStart(); it != macroInfo->getMacroEnd(); ++it) {
					/* Check if the macro already exists before adding it. */
					bool alreadyExists = false;
					for (vector<Macro>::iterator it2 = ns->_macros.begin(); it2 != ns->_macros.end(); ++it2) {
						if (it2->_name == it->first) {
							alreadyExists = true;
							break;
						}
					}
					if (!alreadyExists) {
						ns->_macros.push_back(Macro(it->first, it->second));
					}
				}
			}
		}
	}

	return rc;
}
