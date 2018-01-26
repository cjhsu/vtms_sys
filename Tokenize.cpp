// ---------------------------------------------------------------------------
#include <string>
#include <vector>
#pragma hdrstop

#include "Tokenize.h"
// ---------------------------------------------------------------------------
#pragma package(smart_init)

vector<string>Tokenize(const string& str, const string& delimiters) {
	string client = str;
	vector<string>result;

	while (!client.empty()) {
		string::size_type dPos = client.find_first_of(delimiters);
		if (dPos == 0) { // head is delimiter
			client = client.substr(delimiters.length());
			// remove header delimiter
			result.push_back("");
		}
		else { // head is a real node
			string::size_type dPos = client.find_first_of(delimiters);
			string element = client.substr(0, dPos);
			result.push_back(element);

			if (dPos == string::npos)
			{ // node is last element, no more delimiter
				return result;
			}
			else {
				// Modifyed by Charles Hsu
				int len = dPos + delimiters.length();
				if (len > (int)client.length())
					len = client.length();
				//
				client = client.substr(len);
			}
		}
	}
	if (client.empty()) { // last element is delimeter
		result.push_back("");
	}
	return result;
}
