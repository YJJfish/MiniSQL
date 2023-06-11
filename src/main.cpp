#include <iostream>
#include <string>

#include "Global.h"
#include "Exception.h"
#include "BufferManager.h"
#include "IndexManager.h"
#include "RecordManager.h"
#include "CatalogManager.h"
#include "API.h"
#include "Interpreter.h"

using namespace std;

BufferManager bm;
IndexManager im(bm);
RecordManager rm(bm);
CatalogManager cm;
API api(cm, rm, im);
Interpreter interpreter(api, cm);

int main() {
	while (1) {
		string cmd;
		getline(cin, cmd);
		const ReturnInfo& ReturnValue = interpreter.ExecCommand(cmd);
		cout << ReturnValue.Message << endl;
		if (ReturnValue.Quit)
			break;
	}

}