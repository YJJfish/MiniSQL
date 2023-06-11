#include <iostream>
#include <stdio.h>
#include "Global.h"
#include "Exception.h"
#include "RecordManager.h"


using namespace std;

void RecordManager::CreateTable(const TableSchema& table) {
	string TableFile = DataDirectory + table.TableName + ".Table";
	xhd(table);
}

void RecordManager::DropTable(const string& TableName) {
	string filename = DataDirectory + TableName + ".Table";
	Remove(filename);
}

Pos* RecordManager::Intersect(Pos* table1, Pos* table2) {
	Pos DummyHead;
	Pos* ptr = &DummyHead;
	while (table1 && table2)
		if (table1->BlockOffset == table2->BlockOffset && table1->ByteOffset == table2->ByteOffset) {
			ptr = ptr->Next = table1;
			table1 = table1->Next;
			Pos* temp = table2->Next;
			delete table2;
			table2 = temp;
		}
		else if (table1->BlockOffset < table2->BlockOffset || table1->BlockOffset == table2->BlockOffset && table1->ByteOffset < table2->ByteOffset) {
			Pos* temp = table1->Next;
			delete table1;
			table1 = temp;
		}
		else {
			Pos* temp = table2->Next;
			delete table2;
			table2 = temp;
		}
	while (table1) {
		Pos* temp = table1->Next;
		delete table1;
		table1 = temp;
	}
	while (table2) {
		Pos* temp = table2->Next;
		delete table2;
		table2 = temp;
	}
	return DummyHead.Next;
}

int RecordManager::Select(const TableSchema& table, Pos* temptable,
	int length, string AttrName[], int op[], void* values[],
	const string& ResultFile)
{
	string FilePath = DataDirectory + table.TableName + ".Table";
	ofstream fout(ResultFile);
	int count = 0;
	unsigned int RecordSize = table.RecordSize();
	//输出查询结果的列名
	PrintAttr(table, fout);
	for (Pos* p = temptable; p; p = p->Next) {
		Block* B = bm.GetBlock(FilePath, p->BlockOffset);
		void* v = NULL;
		int AttrIndex;
		bool flag = true;
		for (int i = 0; i < length; i++) {
			for (int j = 0; j < table.NumAttributes; j++)
				if (AttrName[i] == table.AttributeName[j]) {
					AttrIndex = j;
					if (table.Type[j] == SQL_INT) {
						v = new int;
						memcpy(v, B->Content + (p->ByteOffset) + table.AttrOffset(j), table.AttrSize(j));
					}
					else if (table.Type[j] == SQL_FLOAT) {
						v = new float;
						memcpy(v, B->Content + (p->ByteOffset) + table.AttrOffset(j), table.AttrSize(j));
					}
					else {
						char buffer[1024];
						memcpy(buffer, B->Content + (p->ByteOffset) + table.AttrOffset(j), table.AttrSize(j));
						buffer[table.AttrSize(j)] = 0;
						v = new string(buffer);
					}
					break;
				}
			switch (op[i]) {
			case eq:
				if (Compare(v, values[i], table.Type[AttrIndex]) != 0) flag = false; break;
			case neq:
				if (Compare(v, values[i], table.Type[AttrIndex]) == 0) flag = false; break;
			case les:
				if (Compare(v, values[i], table.Type[AttrIndex]) >= 0) flag = false; break;
			case greater:
				if (Compare(v, values[i], table.Type[AttrIndex]) <= 0) flag = false; break;
			case leq:
				if (Compare(v, values[i], table.Type[AttrIndex]) == 1) flag = false; break;
			case geq:
				if (Compare(v, values[i], table.Type[AttrIndex]) == -1) flag = false; break;
			}
			delete v;
			if (flag == false)
				break;
		}
		if (flag) {
			count++;
			string s = "[";
			string val;
			for (int k = 0; k < table.NumAttributes; k++) {
				s += "\"";
				if (table.Type[k] == SQL_INT) {
					int Value = 0;
					memcpy(&Value, B->Content + (p->ByteOffset) + table.AttrOffset(k), table.AttrSize(k));
					val = to_string(Value);
				}
				else if (table.Type[k] == SQL_FLOAT) {
					float Value = 0.0;
					memcpy(&Value, B->Content + (p->ByteOffset) + table.AttrOffset(k), table.AttrSize(k));
					val = to_string(Value);
				}
				else {
					char buffer[1024];
					memcpy(buffer, B->Content + (p->ByteOffset) + table.AttrOffset(k), table.AttrSize(k));
					buffer[table.AttrSize(k)] = 0;
					val = buffer;
				}
				s += val + "\",";
			}
			s[s.length() - 1] = ']';
			fout << s << endl;
		}
	}
	fout.close();
	return count;
}

int RecordManager::Select(const TableSchema& table,
	int length, string AttrName[], int op[], void* values[],
	const string& ResultFile)
{
	ofstream fout(ResultFile);
	int count = 0;
	string TableFile = DataDirectory + table.TableName + ".Table";
	void* v = NULL;
	unsigned int RecordSize = table.RecordSize();
	unsigned int RecordNum = BlockSize / RecordSize;
	unsigned int TableSize = FileSize(TableFile);
	int AttrIndex;
	//输出查询结果的列名
	PrintAttr(table, fout);
	//遍历表数据文件
	for (unsigned int x = 0; x < TableSize; x++) {
		//取得第x块元组数据
		Block* B = bm.GetBlock(TableFile, x);
		for (unsigned int y = 0; y < RecordNum; y++) {
			//若当前元组为空, 跳过
			if (B->Content[y * RecordSize] == 0) continue;
			bool flag = true;
			//i遍历每个条件
			for (int i = 0; i < length; i++) {
				//j遍历每个属性, 把第i个条件对应的属性找出来
				for (int j = 0; j < table.NumAttributes; j++)
					if (AttrName[i] == table.AttributeName[j]) {
						AttrIndex = j;
						if (table.Type[j] == SQL_INT) {
							v = new int;
							memcpy(v, B->Content + y * RecordSize + table.AttrOffset(j), table.AttrSize(j));
						}
						else if (table.Type[j] == SQL_FLOAT) {
							v = new float;
							memcpy(v, B->Content + y * RecordSize + table.AttrOffset(j), table.AttrSize(j));
						}
						else {
							char buffer[1024];
							memcpy(buffer, B->Content + y * RecordSize + table.AttrOffset(j), table.AttrSize(j));
							buffer[table.AttrSize(j)] = 0;
							v = new string(buffer);
						}
						break;
					}
				switch (op[i]) {
				case eq:
					if (Compare(v, values[i], table.Type[AttrIndex]) != 0) flag = false; break;
				case neq:
					if (Compare(v, values[i], table.Type[AttrIndex]) == 0) flag = false; break;
				case les:
					if (Compare(v, values[i], table.Type[AttrIndex]) >= 0) flag = false; break;
				case greater:
					if (Compare(v, values[i], table.Type[AttrIndex]) <= 0) flag = false; break;
				case leq:
					if (Compare(v, values[i], table.Type[AttrIndex]) == 1) flag = false; break;
				case geq:
					if (Compare(v, values[i], table.Type[AttrIndex]) == -1) flag = false; break;
				}
				delete v;
				if (flag == false) break;
			}
			if (flag) {
				count++;
				string val;
				string s = "[";
				for (int k = 0; k < table.NumAttributes; k++) {
					s += "\"";
					if (table.Type[k] == SQL_INT) {
						int Value = 0;
						memcpy(&Value, B->Content + y * RecordSize + table.AttrOffset(k), table.AttrSize(k));
						val = to_string(Value);
					}
					else if (table.Type[k] == SQL_FLOAT) {
						float Value = 0.0;
						memcpy(&Value, B->Content + y * RecordSize + table.AttrOffset(k), table.AttrSize(k));
						val = to_string(Value);
					}
					else {
						char buffer[1024];
						memcpy(buffer, B->Content + y * RecordSize + table.AttrOffset(k), table.AttrSize(k));
						buffer[table.AttrSize(k)] = 0;
						val = buffer;
					}
					s += val + "\",";
				}
				s[s.length() - 1] = ']';
				fout << s << endl;
			}
		}
	}
	fout.close();
	return count;
}

Pos RecordManager::Insert(const TableSchema& table, void* values[]) {
	Pos ReturnValue;
	string TableFile = DataDirectory + table.TableName + ".Table";
	Block* B0 = bm.GetBlock(TableFile, 0);
	unsigned short byteoffset, blockoffset, tembyte, temblock;
	memcpy(&byteoffset, B0->Content + 3, 2);
	memcpy(&blockoffset, B0->Content + 1, 2);
	if (!(blockoffset || byteoffset)) {
		xhd(table);
		memcpy(&blockoffset, B0->Content + 1, 2);
		memcpy(&byteoffset, B0->Content + 3, 2);
	}
	Block* Bn = bm.GetBlock(TableFile, blockoffset);
	memcpy(&tembyte, Bn->Content + byteoffset + 3, 2);
	memcpy(&temblock, Bn->Content + byteoffset + 1, 2);
	memcpy(B0->Content + 3, &tembyte, 2);
	memcpy(B0->Content + 1, &temblock, 2);
	memset(Bn->Content + byteoffset, 1, 1);
	for (int i = 0; i < table.NumAttributes; i++)
		if (table.Type[i] == SQL_INT || table.Type[i] == SQL_FLOAT)
			memcpy(Bn->Content + byteoffset + table.AttrOffset(i), values[i], table.AttrSize(i));
		else
			memcpy(Bn->Content + byteoffset + table.AttrOffset(i), ((string*)values[i])->c_str(), table.AttrSize(i));
	bm.VisitBlock(B0, 1);
	bm.VisitBlock(Bn, 1);
	ReturnValue.BlockOffset = blockoffset;
	ReturnValue.ByteOffset = byteoffset;
	ReturnValue.Next = NULL;
	return ReturnValue;
}

bool RecordManager::Conflict(const TableSchema& table, void* values[]) {
	string TableFile = DataDirectory + table.TableName + ".Table";
	unsigned short RecordSize = table.RecordSize();
	unsigned int RecordNum = BlockSize / RecordSize;
	unsigned int TableSize = FileSize(TableFile);
	for (int i = 0; i < TableSize; i++) {
		//取得第i块元组数据
		Block* B1 = bm.GetBlock(TableFile, i);
		for (int j = 0; j < RecordNum; j++)
			//若当前元组不为空
			if (B1->Content[j * RecordSize])
				for (int k = 0; k < table.NumAttributes; k++)
					if (table.Unique[k]) {
						void* ptr;
						if (table.Type[k] == SQL_INT) {
							ptr = new int;
							memcpy(ptr, B1->Content + j * RecordSize + table.AttrOffset(k), table.AttrSize(k));
						}
						else if (table.Type[k] == SQL_FLOAT) {
							ptr = new float;
							memcpy(ptr, B1->Content + j * RecordSize + table.AttrOffset(k), table.AttrSize(k));
						}
						else {
							char Buffer[1024];
							memcpy(Buffer, B1->Content + j * RecordSize + table.AttrOffset(k), table.AttrSize(k));
							Buffer[table.AttrSize(k)] = '\0';
							ptr = new string(Buffer);
						}
						if (Compare(values[k], ptr, table.Type[k]) == 0) {
							string ConflictValue;
							if ((table.Type[k] == SQL_INT)) ConflictValue = to_string(*(int*)ptr);
							else if ((table.Type[k] == SQL_FLOAT)) ConflictValue = to_string(*(float*)ptr);
							else ConflictValue = *(string*)ptr;
							delete ptr;
							throw InsertionError(table.AttributeName[k], ConflictValue);
						}
						delete ptr;
					}
	}
	return true;
}

DeletedTuple* RecordManager::Delete(const TableSchema& table, Pos* temptable,
	int length, string AttrName[], int op[], void* value[])
{
	DeletedTuple* D = NULL;
	string FilePath = DataDirectory + table.TableName + ".Table";
	int count = 0;
	unsigned int RecordSize = table.RecordSize();
	for (Pos* p = temptable; p; p = p->Next) {
		Block* B = bm.GetBlock(FilePath, p->BlockOffset);
		void* v = NULL;
		int AttrIndex;
		bool flag = true;
		for (int i = 0; i < length; i++) {
			for (int j = 0; j < table.NumAttributes; j++)
				if (AttrName[i] == table.AttributeName[j]) {
					AttrIndex = j;
					if (table.Type[j] == SQL_INT) {
						v = new int;
						memcpy(v, B->Content + (p->ByteOffset) + table.AttrOffset(j), table.AttrSize(j));
					}
					else if (table.Type[j] == SQL_FLOAT) {
						v = new float;
						memcpy(v, B->Content + (p->ByteOffset) + table.AttrOffset(j), table.AttrSize(j));
					}
					else {
						char buffer[1024];
						memcpy(buffer, B->Content + (p->ByteOffset) + table.AttrOffset(j), table.AttrSize(j));
						buffer[table.AttrSize(j)] = 0;
						v = new string(buffer);
					}
					break;
				}
			switch (op[i]) {
			case eq:
				if (Compare(v, value[i], table.Type[AttrIndex]) != 0) flag = false; break;
			case neq:
				if (Compare(v, value[i], table.Type[AttrIndex]) == 0) flag = false; break;
			case les:
				if (Compare(v, value[i], table.Type[AttrIndex]) >= 0) flag = false; break;
			case greater:
				if (Compare(v, value[i], table.Type[AttrIndex]) <= 0) flag = false; break;
			case leq:
				if (Compare(v, value[i], table.Type[AttrIndex]) == 1) flag = false; break;
			case geq:
				if (Compare(v, value[i], table.Type[AttrIndex]) == -1) flag = false; break;
			}
			delete v;
			if (flag == false)
				break;
		}
		if (flag) {
			DeletedTuple* q = new DeletedTuple;
			if (D == NULL) {
				D = q;
				q->Next = NULL;
			}
			else {
				q->Next = D->Next;
				D->Next = q;
			}
			//存储这个元组的信息
			q->values = new void* [table.NumAttributes];
			for (int i = 0; i < table.NumAttributes; i++)
				if (table.Type[i] == SQL_INT) {
					q->values[i] = new int;
					memcpy(q->values[i], B->Content + (p->ByteOffset) + table.AttrOffset(i), table.AttrSize(i));
				}
				else if (table.Type[i] == SQL_FLOAT) {
					q->values[i] = new float;
					memcpy(q->values[i], B->Content + (p->ByteOffset) + table.AttrOffset(i), table.AttrSize(i));
				}
				else {
					char buffer[1024];
					memcpy(buffer, B->Content + (p->ByteOffset) + table.AttrOffset(i), table.AttrSize(i));
					buffer[table.AttrSize(i)] = 0;
					q->values[i] = new string(buffer);
				}
			//在表中删除这个元组
			memset(B->Content + (p->ByteOffset), 0, 1);
			Block* B0 = bm.GetBlock(FilePath, 0);
			memcpy(B->Content + (p->ByteOffset) + 1, B0->Content + 1, 2);
			memcpy(B->Content + (p->ByteOffset) + 3, B0->Content + 3, 2);
			memcpy(B0->Content + 1, &(p->BlockOffset), 2);
			memcpy(B0->Content + 3, &(p->ByteOffset), 2);
			bm.VisitBlock(B0, 1);
			bm.VisitBlock(B, 1);
		}
	}
	return D;
}

DeletedTuple* RecordManager::Delete(const TableSchema& table,
	int length, string AttrName[], int op[], void* value[])
{
	DeletedTuple* D = NULL;
	int count = 0;
	string TableFile = DataDirectory + table.TableName + ".Table";
	void* v = NULL;
	unsigned int RecordSize = table.RecordSize();
	unsigned int RecordNum = (BlockSize) / RecordSize;
	unsigned int TableSize = FileSize(TableFile);
	int AttrIndex;
	//遍历表数据文件
	for (unsigned short x = 0; x < TableSize; x++) {
		//取得第x块元组数据
		Block* B = bm.GetBlock(TableFile, x);
		for (unsigned short y = 0; y < RecordNum; y++) {
			//若当前元组为空, 跳过
			if (B->Content[y * RecordSize] == 0) continue;
			bool flag = true;
			for (int i = 0; i < length; i++) {
				for (int j = 0; j < table.NumAttributes; j++)
					if (AttrName[i] == table.AttributeName[j]) {
						AttrIndex = j;
						if (table.Type[j] == SQL_INT) {
							v = new int;
							memcpy(v, B->Content + y * RecordSize + table.AttrOffset(j), table.AttrSize(j));
						}
						else if (table.Type[j] == SQL_FLOAT) {
							v = new float;
							memcpy(v, B->Content + y * RecordSize + table.AttrOffset(j), table.AttrSize(j));
						}
						else {
							char buffer[1024];
							memcpy(buffer, B->Content + y * RecordSize + table.AttrOffset(j), table.AttrSize(j));
							buffer[table.AttrSize(j)] = 0;
							v = new string(buffer);
						}
						break;
					}
				switch (op[i]) {
				case eq:
					if (Compare(v, value[i], table.Type[AttrIndex]) != 0) flag = false; break;
				case neq:
					if (Compare(v, value[i], table.Type[AttrIndex]) == 0) flag = false; break;
				case les:
					if (Compare(v, value[i], table.Type[AttrIndex]) >= 0) flag = false; break;
				case greater:
					if (Compare(v, value[i], table.Type[AttrIndex]) <= 0) flag = false; break;
				case leq:
					if (Compare(v, value[i], table.Type[AttrIndex]) == 1) flag = false; break;
				case geq:
					if (Compare(v, value[i], table.Type[AttrIndex]) == -1) flag = false; break;
				}
				delete v;
				if (flag == false) break;
			}
			if (flag) {
				DeletedTuple* q = new DeletedTuple;
				if (D == NULL) {
					D = q;
					q->Next = NULL;
				}
				else {
					q->Next = D->Next;
					D->Next = q;
				}
				//存储这个元组的信息
				q->values = new void* [table.NumAttributes];
				for (int i = 0; i < table.NumAttributes; i++)
					if (table.Type[i] == SQL_INT) {
						q->values[i] = new int;
						memcpy(q->values[i], B->Content + y * RecordSize + table.AttrOffset(i), table.AttrSize(i));
					}
					else if (table.Type[i] == SQL_FLOAT) {
						q->values[i] = new float;
						memcpy(q->values[i], B->Content + y * RecordSize + table.AttrOffset(i), table.AttrSize(i));
					}
					else {
						char buffer[1024];
						memcpy(buffer, B->Content + y * RecordSize + table.AttrOffset(i), table.AttrSize(i));
						buffer[table.AttrSize(i)] = 0;
						q->values[i] = new string(buffer);
					}
				//在表中删除这个元组
				memset(B->Content + y * RecordSize, 0, 1);
				Block* B0 = bm.GetBlock(TableFile, 0);
				memcpy(B->Content + y * RecordSize + 1, B0->Content + 1, 2);
				memcpy(B->Content + y * RecordSize + 3, B0->Content + 3, 2);
				memcpy(B0->Content + 1, &x, 2);
				unsigned short temp = y * RecordSize;
				memcpy(B0->Content + 3, &temp, 2);
				bm.VisitBlock(B0, 1);
				bm.VisitBlock(B, 1);
			}
		}
	}
	return D;
}


void RecordManager::xhd(const TableSchema& table) {
	unsigned int RecordSize = table.RecordSize();
	string TableFile = DataDirectory + table.TableName + ".Table";
	Block* New = bm.AppendBlock(TableFile);
	unsigned short RecordNum = (BlockSize) / RecordSize;
	unsigned short blockoffset = New->GetOffset();
	for (unsigned short i = 0; i < RecordNum - 1; i++) {
		memset(New->Content + RecordSize * i, 0, 1);
		memcpy(New->Content + RecordSize * i + 1, &blockoffset, 2);
		unsigned short temp = (i + 1) * RecordSize;
		memcpy(New->Content + RecordSize * i + 3, &temp, 2);
	}
	memset(New->Content + RecordSize * (RecordNum - 1), 0, 1);
	memset(New->Content + RecordSize * (RecordNum - 1) + 1, 0, 2);
	memset(New->Content + RecordSize * (RecordNum - 1) + 3, 0, 2);
	bm.VisitBlock(New, 1);
	if (blockoffset) {
		Block* B = bm.GetBlock(TableFile, 0);
		memcpy(B->Content + 1, &blockoffset, 2);
		memset(B->Content + 3, 0, 2);
		bm.VisitBlock(B, 1);
	}
	return;
}

void RecordManager::PrintAttr(const TableSchema& table, ofstream& fout) {
	fout << "[";
	for (int i = 0; i < table.NumAttributes; i++) {
		fout << "\"" << table.AttributeName[i] << " ";
		if (table.Type[i] == SQL_INT) fout << "int";
		else if (table.Type[i] == SQL_FLOAT) fout << "float";
		else fout << "char(" << (int)table.Length[i] << ")";
		fout << "\"";
		if (i == table.NumAttributes - 1)
			fout << "]" << endl;
		else
			fout << ",";
	}
}