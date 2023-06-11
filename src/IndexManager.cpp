#include <iostream>
#include <fstream>

#include "Global.h"
#include "Exception.h"
#include "IndexManager.h"

using namespace std;

class TempRecord {
public:
	unsigned short BlockOffset;
	unsigned short ByteOffset;
	void* Value;
	TempRecord* Next;
	TempRecord(int BlockOffset, int ByteOffset, void* Value) :BlockOffset(-1), ByteOffset(-1), Value(Value), Next(0) {}
	~TempRecord(void) { if (Value) delete Value; }
};

static void QuickSort(TempRecord* Arr[], int l, int r, unsigned char Type) {
	void* m = Arr[(l + r) / 2]->Value;
	int i = l, j = r;
	while (i <= j) {
		while (Compare(Arr[i]->Value, m, Type) == -1)i++;
		while (Compare(m, Arr[j]->Value, Type) == -1)j--;
		if (i <= j) {
			TempRecord* temp;
			temp = Arr[i];
			Arr[i] = Arr[j];
			Arr[j] = temp;
			i++; j--;
		}
	}
	if (l < j) QuickSort(Arr, l, j, Type);
	if (i < r) QuickSort(Arr, i, r, Type);
}

static void QuickSort(Pos* Arr[], int l, int r) {
	Pos* m = Arr[(l + r) / 2];
	int i = l, j = r;
	while (i <= j) {
		while ((Arr[i]->BlockOffset < m->BlockOffset) || (Arr[i]->BlockOffset == m->BlockOffset && Arr[i]->ByteOffset < m->ByteOffset)) i++;
		while ((Arr[j]->BlockOffset > m->BlockOffset) || (Arr[j]->BlockOffset == m->BlockOffset && Arr[j]->ByteOffset > m->ByteOffset)) j--;
		if (i <= j) {
			Pos* temp;
			temp = Arr[i];
			Arr[i] = Arr[j];
			Arr[j] = temp;
			i++; j--;
		}
	}
	if (l < j) QuickSort(Arr, l, j);
	if (i < r) QuickSort(Arr, i, r);
}


//��������, ����table�����е����ݹ���B+��
//B+��ÿ�����Ľṹ:
// 1 Byte: ָʾ��ǰ�Ƿ���Ҷ�ڵ�.
// 2 Bytes: ָʾ��ǰ��ռ����. �����Ҷ�ڵ�, �����Ԫ�����, ����FanOut-1; ����Ƿ�Ҷ���, ������ӽ�����, ����FanOut.
// 2 Bytes: �����б�ָ��. ���ҽ���Ŀǰ�ǵ�0�顢��ռ����=0ʱ, ���ָ���������. 
// FanOut*4 Bytes: FanOut��ָ��. �����Ҷ�ڵ�, ��ÿ��ָ��=(��ƫ����, �ֽ�ƫ����)ָ��Ԫ��λ��;
//								 ����Ƿ�Ҷ���, ��ÿ��ָ��=��ƫ����, ָ����һ�����Ŀ�.
// (FanOut-1)*m Bytes: (FanOut-1)��Ԫ��ֵ, ����m=AttrSize
void IndexManager::CreateIndex(const TableSchema& table, const IndexSchema& index) {
	//��ʼ������
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	string TableFile = DataDirectory + index.TableName + ".Table";
	unsigned int AttrSize = index.AttrSize();
	unsigned int AttrOffset = table.AttrOffset(index.AttributeNumber);
	unsigned int RecordSize = table.RecordSize();
	unsigned int RecordNum = BlockSize / RecordSize;
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	unsigned int TableSize = FileSize(TableFile);
	//�����������ļ�
	unsigned short RecordCnt = 0;
	TempRecord lst(0, 0, 0);//��ͷָ��
	TempRecord* ptr = &lst;
	for (unsigned int i = 0; i < TableSize; i++) {
		//ȡ�õ�i��Ԫ������
		Block* TableBlock = bm.GetBlock(TableFile, i);
		for (unsigned int j = 0; j < RecordNum; j++) {
			//����ǰԪ��Ϊ��, ����
			if (TableBlock->Content[j * RecordSize] == 0)
				continue;
			//����, ����ǰԪ���λ�á�����ֵ����Ҷ�ڵ�
			if (index.Type == SQL_INT) {
				int Value = 0;
				memcpy(&Value, TableBlock->Content + j * RecordSize + AttrOffset, AttrSize);
				ptr->Next = new TempRecord(i, j * RecordSize, new int(Value));
			}
			else if (index.Type == SQL_FLOAT) {
				float Value = 0.0;
				memcpy(&Value, TableBlock->Content + j * RecordSize + AttrOffset, AttrSize);
				ptr->Next = new TempRecord(i, j * RecordSize, new float(Value));
			}
			else {
				char buffer[1024];
				memcpy(buffer, TableBlock->Content + j * RecordSize + AttrOffset, AttrSize);
				buffer[AttrSize] = 0;
				ptr->Next = new TempRecord(i, j * RecordSize, new string(buffer));
			}
			ptr = ptr->Next;
			ptr->BlockOffset = i;
			ptr->ByteOffset = j * RecordSize;
			RecordCnt++;
		}
	}
	//����ǿձ�
	if (RecordCnt == 0) {
		//�������ļ�
		Block* IndexBlock = bm.AppendBlock(IndexFile);
		//�޸��ļ�����
		IndexBlock->Content[0] = 1;//Ҷ�ڵ�
		IndexBlock->Content[1] = IndexBlock->Content[2] = 0;//ռ����Ϊ0
		IndexBlock->Content[3] = IndexBlock->Content[4] = 0;//�����б�ָ��=NULL
		//����д���
		bm.VisitBlock(IndexBlock, 1);
		return;
	}
	//����������
	TempRecord** Arr = new TempRecord * [RecordCnt];
	ptr = lst.Next;
	for (int i = 0; i < RecordCnt; i++) {
		Arr[i] = ptr;
		ptr = ptr->Next;
	}
	QuickSort(Arr, 0, RecordCnt - 1, index.Type);
	/*cout << "������:" << endl;
	for (int i = 0; i < RecordCnt; i++) {
		cout << "Value:" << ((string*)(Arr[i]->Value))->c_str() << " BlockOffset: " << Arr[i]->BlockOffset << " ByteOffset: " << Arr[i]->ByteOffset << endl;
	}*/
	//����, �������Ԫ��ȫ����Arr��, ����ΪRecordCnt
	if (RecordCnt <= FanOut - 1) {//��ֱ�Ӵ���һ��Ҷ�ڵ�
		//�������ļ�
		Block* IndexBlock = bm.AppendBlock(IndexFile);
		//�޸��ļ�����
		IndexBlock->Content[0] = 1;//Ҷ�ڵ�
		memcpy(IndexBlock->Content + 1, &RecordCnt, 2);//ռ����ΪRecordCnt
		IndexBlock->Content[3] = IndexBlock->Content[4] = 0;//�����б�ָ��=NULL
		//�洢Arr�е�������Ϣ
		for (int i = 0; i < RecordCnt; i++) {
			memcpy(IndexBlock->Content + 5 + i * (4 + AttrSize), &(Arr[i]->BlockOffset), 2);
			memcpy(IndexBlock->Content + 7 + i * (4 + AttrSize), &(Arr[i]->ByteOffset), 2);
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(IndexBlock->Content + 9 + i * (4 + AttrSize), Arr[i]->Value, AttrSize);
			else
				memcpy(IndexBlock->Content + 9 + i * (4 + AttrSize), ((string*)Arr[i]->Value)->c_str(), AttrSize);
		}
		//����ʣ�µĿռ�
		if (RecordCnt < FanOut - 1)
			memset(IndexBlock->Content + 5 + RecordCnt * (4 + AttrSize), 0, (4 + AttrSize) * (FanOut - 1 - RecordCnt));
		//����ĩβָ��
		memset(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), 0, 4);
		//����д���
		bm.VisitBlock(IndexBlock, 1);
		//�ͷ�Arr��������ڴ�ռ�
		for (int i = 0; i < RecordCnt; i++)
			delete Arr[i];
		delete[] Arr;
	}
	else {//һ��Ҷ�ڵ�治�����м�¼
		unsigned int LevelStart = 1;//B+����ǰ�����ʼ��
		unsigned int LevelCount = 0;//B+����ǰ���ж��ٿ�
		unsigned short Occupancy = FanOut - 1;
		unsigned short Expected = FanOut - 1;
		Block* IndexBlock = bm.AppendBlock(IndexFile);//��0��, ��ʱ����
		for (unsigned int i = 0; i < RecordCnt; i++) {//�������Ҷ�ڵ�, һ��Ҫռ��RecordCnt / (FanOut - 1)��ȡ����Ҷ�ڵ�
			if (Occupancy == Expected) {//��һ������, Ҫ������һ��Ĵ洢�ռ�
				LevelCount++;
				Occupancy = 0;
				IndexBlock = bm.AppendBlock(IndexFile);//��ǰ�ǵ�LevelStart + LevelCount - 1��
				//����Ҷ�ڵ��־
				IndexBlock->Content[0] = 1;
				//����ռ���ʺ�ĩβָ��
				if (RecordCnt - i <= FanOut - 1) {//��ʣRecordCnt - i����¼, С�ڵ���FanOut - 1, ˵���������һ��Ҷ�ڵ�
					Expected = RecordCnt - i;
					memcpy(IndexBlock->Content + 1, &Expected, 2);
					memset(IndexBlock->Content + 3, 0, 2);
					memset(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), 0, 4);
				}
				else if (RecordCnt - i < (FanOut - 1) + FanOut / 2){//��ʣRecordCnt - i����¼, ����(FanOut - 1), ��С��(FanOut - 1)+(FanOut - 1)/2��ȡ��, ˵��ʣ����Ҫ���2��Ҷ�ڵ�
					Expected = (RecordCnt - i) / 2;
					memcpy(IndexBlock->Content + 1, &Expected, 2);
					memset(IndexBlock->Content + 3, 0, 2);
					unsigned int temp1 = LevelStart + LevelCount;
					memcpy(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), &temp1, 4);
				}
				else {//��ʣRecordCnt - i����¼, ���ڵ���(FanOut - 1)+(FanOut - 1)/2��ȡ��, ���������Ǹ�Ҷ�ڵ�ֱ������
					Expected = FanOut - 1;
					memcpy(IndexBlock->Content + 1, &Expected, 2);
					memset(IndexBlock->Content + 3, 0, 2);
					unsigned int temp1 = LevelStart + LevelCount;
					memcpy(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), &temp1, 4);
				}
				//����д���
				bm.VisitBlock(IndexBlock, 1);
			}
			//����ǰԪ����뵱ǰ��
			memcpy(IndexBlock->Content + 5 + Occupancy * (4 + AttrSize), &(Arr[i]->BlockOffset), 2);
			memcpy(IndexBlock->Content + 7 + Occupancy * (4 + AttrSize), &(Arr[i]->ByteOffset), 2);
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(IndexBlock->Content + 9 + Occupancy * (4 + AttrSize), Arr[i]->Value, AttrSize);
			else
				memcpy(IndexBlock->Content + 9 + Occupancy * (4 + AttrSize), ((string*)Arr[i]->Value)->c_str(), AttrSize);
			//�����������Ԫ��
			if (Occupancy) {//ֻ�е�ǰ��ĵ�0����¼����Ϣ����Ҫ����, �����ͷ�
				delete Arr[i];
				Arr[i] = 0;
			}
			Occupancy++;
		}
		unsigned int LastLevelCount = RecordCnt;
		while (LevelCount > 1) {//���򹹽�, ֱ��LevelCount = 1, ˵����ǰ��ֻʣһ�����, Ҳ���Ǹ��ڵ�
			//�Ȱ���һ��ʣ�µ�LevelCount�������㱣�浽����
			for (unsigned int i = 0, j = 0; i < LastLevelCount; i++)
				if (Arr[i]) {
					TempRecord* temp = Arr[i];
					Arr[i] = 0;
					Arr[j] = temp;
					j++;
				}
			//��ʼ����
			unsigned int NewLevelStart = LevelStart + LevelCount;
			unsigned int NewLevelCount = 0;
			unsigned short Occupancy = FanOut;
			unsigned short Expected = FanOut;
			for (unsigned int i = 0; i < LevelCount; i++) {
				if (Occupancy == Expected) {//��һ������, Ҫ������һ��Ĵ洢�ռ�
					NewLevelCount++;
					Occupancy = 0;
					//��LevelCount > FanOut, ��ǰ�ǵ�NewLevelStart + NewLevelCount - 1��; ����ֱ��ȡ��0����Ϊ���ڵ�
					IndexBlock = (LevelCount > FanOut) ? bm.AppendBlock(IndexFile) : bm.GetBlock(IndexFile, 0);
					//���÷�Ҷ�ڵ��־
					IndexBlock->Content[0] = 0;
					//����ռ����
					if (LevelCount - i <= FanOut) {//��ʣLevelCount - i����¼, С�ڵ���FanOut, ˵�����ǵ�ǰ�����һ���ڵ�
						Expected = LevelCount - i;
						memcpy(IndexBlock->Content + 1, &Expected, 2);
						memset(IndexBlock->Content + 3, 0, 2);
					}
					else if (LevelCount - i < FanOut + (FanOut + 1) / 2) {//��ʣLevelCount - i����¼, ����FanOut, ��С��FanOut+FanOut/2��ȡ��, ˵��ʣ����Ҫ����������
						Expected = (LevelCount - i) / 2;
						memcpy(IndexBlock->Content + 1, &Expected, 2);
						memset(IndexBlock->Content + 3, 0, 2);
					}
					else {//��ʣLevelCount - i����¼, ���ڵ���FanOut+FanOut/2��ȡ��, ���������Ǹ�Ҷ�ڵ�ֱ������
						Expected = FanOut;
						memcpy(IndexBlock->Content + 1, &Expected, 2);
						memset(IndexBlock->Content + 3, 0, 2);
					}
					//����д���
					bm.VisitBlock(IndexBlock, 1);
				}
				//����ǰԪ����뵱ǰ��
				unsigned int temp = LevelStart + i;
				memcpy(IndexBlock->Content + 5 + Occupancy * (4 + AttrSize), &temp, 4);
				if (Occupancy) {//��Ҷ�����ֵ��ʾ�ָ�ֵ, �����0��ֵ��ʾ��0�������͵�1�������ķָ�ֵ. ����Ҫ��1��ʼ
					if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
						memcpy(IndexBlock->Content + 5 + Occupancy * (4 + AttrSize) - AttrSize, Arr[i]->Value, AttrSize);
					else
						memcpy(IndexBlock->Content + 5 + Occupancy * (4 + AttrSize) - AttrSize, ((string*)Arr[i]->Value)->c_str(), AttrSize);
				}
				//�����������Ԫ��
				if (Occupancy) {//ֻ�е�ǰ��ĵ�0����¼����Ϣ����Ҫ����, �����ͷ�
					delete Arr[i];
					Arr[i] = 0;
				}
				Occupancy++;
			}
			//����, ������һ��
			LastLevelCount = LevelCount;
			LevelCount = NewLevelCount;
			LevelStart = NewLevelStart;
		}
		//�ͷ�����������ڴ�
		delete Arr[0];//���ʱ���Ȼֻʣ��һ��������
		delete[] Arr;
	}
}


//ɾ��TableName������������ļ�
void IndexManager::DropIndexOfTable(const string& TableName) {
	Remove(IndexDirectory + TableName + "/");
}


//ɾ����������
void IndexManager::DropIndex(const string& IndexName) {
	DeleteFiles(IndexDirectory, IndexName + ".Index");
}


//��ѯ, ��������. ��֤����Ԫ�ذ�(��ƫ����, �ֽ�ƫ����)����
Pos* IndexManager::Select(const IndexSchema& index,	int length, int op[], void* value[]) {
	//ɨ��������Ϊlength������. ����op, �ѣ����ݵ���������ҳ���, �ѣ����ܵ���С�����ҳ���. ������������ȵģ�����, ֱ�ӷ���NULL. �����Сٵ���������һ������
	void* L = NULL, * LEQ = NULL, * G = NULL, * GEQ = NULL, * EQ = NULL;
	void** NEQ = new void* [length]; int LNEQ = 0;
	for (int i = 0; i < length; i++)
		switch (op[i]) {
		case eq:
			if (EQ && Compare(EQ, value[i], index.Type)) {
				delete[]NEQ;
				return NULL;
			}
			else EQ = value[i];
			break;
		case neq:
			NEQ[LNEQ++] = value[i];
			break;
		case les:
			if (!L || Compare(L, value[i], index.Type) == 1)
				L = value[i];
			break;
		case leq:
			if (!LEQ || Compare(LEQ, value[i], index.Type) == 1)
				LEQ = value[i];
			break;
		case greater:
			if (!G || Compare(G, value[i], index.Type) == -1)
				G = value[i];
			break;
		case geq:
			if (!GEQ || Compare(GEQ, value[i], index.Type) == -1)
				GEQ = value[i];
			break;
		}
	//��ѯ�Ż�, �жϣ����ݡ������ܡ������ٵ������Ľ����Ƿ�Ϊ��, ����ֱ�ӷ���NULL
	//�����͡ݺϲ���һ������
	if (G && GEQ) {
		if (Compare(G, GEQ, index.Type) >= 0)
			GEQ = NULL;//"��"���½���ڵ���"��"���½�, ��"��"Ϊ׼
		else
			G = NULL;//"��"���½�С��"��"���½�, ��"��"Ϊ׼
	}
	//�����ܺ͡ϲ���һ������
	if (L && LEQ) {
		if (Compare(L, LEQ, index.Type) <= 0)
			LEQ = NULL;//"��"���½�С�ڵ���"��"���½�, ��"��"Ϊ׼
		else
			L = NULL;//"��"���½����"��"���½�, ��"��"Ϊ׼
	}
	//�жϣ��ͣ�, ��, ��, ���Ƿ��ͻ
	if ((EQ && G && Compare(EQ, G, index.Type) <= 0)
		|| (EQ && GEQ && Compare(EQ, GEQ, index.Type) == -1)
		|| (EQ && L && Compare(EQ, L, index.Type) >= 0)
		|| (EQ && LEQ && Compare(EQ, LEQ, index.Type) == 1)) {
		delete[]NEQ;
		return NULL;
	}
	//�жϣ��͡��Ƿ��ͻ
	if (EQ)
		for (int i=0;i<LNEQ;i++)
			if (Compare(EQ, NEQ[i], index.Type) == 0) {
				delete[]NEQ;
				return NULL;
			}
	//׼����ʼ��ѯ, �����жϲ���Ŀ��, ȷ����ѯ���Value
	void* Value;
	if (EQ)
		Value = EQ;
	else if (G)
		Value = G;
	else if (GEQ)
		Value = GEQ;
	else
		Value = NULL;//��һ��λ�õ�NULL�ᱻʶ�������С
	//��ʼ������
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	unsigned int AttrSize = index.AttrSize();
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	unsigned int TargetBlock = 0;
	unsigned short Occupancy;
	Block* IndexBlock;
	void* Target;
	if (index.Type == SQL_INT)
		Target = new int;
	else if (index.Type == SQL_FLOAT)
		Target = new float;
	else
		Target = new string;
	//ѭ��ֱ������Ҷ�ڵ�
	while (1) {
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(&Occupancy, IndexBlock->Content + 1, 2);
		//�ж��ǲ���Ҷ�ڵ�
		if (IndexBlock->Content[0] == 1) //��Ҷ�ڵ�
			break;
		unsigned short P = -1;
		for (unsigned short i = 0; i <= Occupancy - 2; i++) {//��Ҷ���ռ����ΪOccupancy, ��ָ�ֵ��Occupancy-1��, �ֱ�λ��IndexBlock->Content[9+i*(AttrSize+4)], i=0,1,...,Occupancy-2
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[4096];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Value, Target, index.Type) == -1) {//�ҵ���һ��Target, �ϸ����Value
				P = i;
				break;
			}
		}
		if (P == 65535)//����ý��������ֵ����Value, ֱ��ȡ���һ��ָ��. ����, ȡ��P��ָ��
			P = Occupancy - 1;
		memcpy(&TargetBlock, IndexBlock->Content + 5 + P * (AttrSize + 4), 4);
	}
	//����Ҷ�ڵ�, ��ʼѰ�Ҹ�ֵ
	unsigned short P = -1;
	for (int i = 0; i<= Occupancy - 1; i++) {
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		if ((!EQ && !GEQ && !G) || (EQ || GEQ) && Compare(Value, Target, index.Type) <= 0 || G && Compare(Value, Target, index.Type) == -1) {
		//����EQ��GEQ�ҵ���һ����Value��ֵ, ����G�ҵ���һ����Value��ֵ
			if (EQ) {//���������ж�����
				if (Compare(Value, Target, index.Type)) { //���������ж�����, �����ʱ��û�ҵ���ȵ�ֵ, ��˵�������Ҳ���Ŀ��ֵ
					delete Target;
					delete[]NEQ;
					return NULL;
				}
				else {//�ҵ�Ŀ��ֵ
					delete Target;
					delete[]NEQ;
					Pos* ReturnValue = new Pos;
					ReturnValue->Next = NULL;
					memcpy(&(ReturnValue->BlockOffset), IndexBlock->Content + 5 + i * (AttrSize + 4), 2);
					memcpy(&(ReturnValue->ByteOffset), IndexBlock->Content + 7 + i * (AttrSize + 4), 2);
					return ReturnValue;
				}
			}
			P = i;//������С�Ĵ��ڵ���Value��ֵ���±�
			break;
		}
	}
	if (P == 65535) {//�����Ҷ�ڵ�����ֵ��С��Value, ˵��Ŀ��ֵ����һ��Ҷ�ڵ���
		memcpy(&TargetBlock, IndexBlock->Content + 5 + (FanOut - 1) * (AttrSize + 4), 4);
		if (TargetBlock == 0) {//��ǰ�Ѿ������һ��Ҷ�ڵ�, û����һ��Ҷ�ڵ�, ����NULL
			delete Target;
			delete[] NEQ;
			return NULL;
		}
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(&Occupancy, IndexBlock->Content + 1, 2);
		P = 0;
	}
	//����, IndexBlock�ĵ�P��ֵӦ�ô洢����С������"��Value"(����GEQ)��"��Value"(����G)��ֵ
	//һֱ�������, ֱ��"��L"��"��LEQ"��һ��������, ������ѭ��
	int Count = 0;
	Pos ReturnValue; Pos* ptr = &ReturnValue;
	while (1) {
		if (P == Occupancy) {
			memcpy(&TargetBlock, IndexBlock->Content + 5 + (FanOut - 1) * (AttrSize + 4), 4);
			if (TargetBlock == 0) //��ǰ�Ѿ������һ��Ҷ�ڵ�, û����һ��Ҷ�ڵ�, break
				break;
			IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
			memcpy(&Occupancy, IndexBlock->Content + 1, 2);
			P = 0;
		}
		//ȡ��IndexBlock��P��ֵ, ����Target
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + P * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + P * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		//"��L"��"��LEQ"��һ��������, ������ѭ��
		if (L && Compare(Target, L, index.Type) >= 0 || LEQ && Compare(Target, LEQ, index.Type) == 1)
			break;
		//��"��"����������, ��������ǰ��
		bool flag = true;
		for(int i=0;i<LNEQ;i++)
			if (Compare(Target, NEQ[i], index.Type) == 0) {
				flag = false;
				break;
			}
		if (flag) {
			Count++;
			ptr->Next = new Pos;
			ptr = ptr->Next;
			memcpy(&(ptr->BlockOffset), IndexBlock->Content + 5 + P * (AttrSize + 4), 2);
			memcpy(&(ptr->ByteOffset), IndexBlock->Content + 7 + P * (AttrSize + 4), 2);
		}
		P++;
	}
	//�Խ������
	Pos** Arr = new Pos * [Count];
	Pos* pos = &ReturnValue;
	for (int i = 0; i < Count; i++)
		Arr[i] = pos = pos->Next;
	QuickSort(Arr, 0, Count - 1);
	for (int i = 0; i < Count; i++)
		Arr[i]->Next = (i == Count - 1) ? NULL : Arr[i + 1];

	delete []NEQ;
	delete Target;
	return Arr[0];
	
}


//���value�Ƿ��Ѿ���B+���д���
bool IndexManager::Conflict(const IndexSchema& index, void* value) {
	const void* Value = value;
	//��ʼ������
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	unsigned int AttrSize = index.AttrSize();
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	//��ʼ����
	unsigned int TargetBlock = 0;
	unsigned short Occupancy;
	Block* IndexBlock;
	void* Target;
	if (index.Type == SQL_INT)
		Target = new int;
	else if (index.Type == SQL_FLOAT)
		Target = new float;
	else
		Target = new string;
	//ѭ��ֱ������Ҷ�ڵ�
	while (1) {
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(&Occupancy, IndexBlock->Content + 1, 2);
		//�ж��ǲ���Ҷ�ڵ�
		if (IndexBlock->Content[0] == 1) //��Ҷ�ڵ�
			break;
		unsigned short P = -1;
		for (unsigned short i = 0; i <= Occupancy - 2; i++) {//��Ҷ���ռ����ΪOccupancy, ��ָ�ֵ��Occupancy-1��, �ֱ�λ��IndexBlock->Content[9+i*(AttrSize+4)], i=0,1,...,Occupancy-2
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[4096];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Value, Target, index.Type) == -1) {//�ҵ���һ��Target, �ϸ����Value
				P = i;
				break;
			}
		}
		if (P == 65535)//����ý��������ֵ����Value, ֱ��ȡ���һ��ָ��. ����, ȡ��P��ָ��
			P = Occupancy - 1;
		memcpy(&TargetBlock, IndexBlock->Content + 5 + P * (AttrSize + 4), 4);
	}
	//����Ҷ�ڵ�, ��ʼѰ�Ҹ�ֵ
	for (int i = Occupancy - 1; i >= 0; i--) {
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		if (Compare(Target, Value, index.Type) == 0) {//����ҵ�
			delete Target;
			return true;
		}
	}
	delete Target;
	return NULL;
}


//��B+���в�����
void IndexManager::Insert(const IndexSchema& index, void* values[], const Pos& position) {
	const void* Value = values[index.AttributeNumber];
	//��ʼ������
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	unsigned int AttrSize = index.AttrSize();
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	//��ʼ����
	//B+�������ж���, ����Index������2^16-1=65535��, �����������15, �ظ��ڵ㵽Ҷ�ڵ�����16����
	unsigned int Track[16] = { 0 };//�Ӹ��ڵ�һֱ׷�ٵ�Ҷ�ڵ�, ��·�Ŀ��
	unsigned short Occup[16] = { 0 };//��·ÿ�����ռ����
	unsigned short Desce[16] = { 0 };//��·��ÿ����ֱ��صڼ���ָ��������
	unsigned short Sibli[16] = { 0 };//�����&���Ѷ��������ֵܽ��
	int TrackLength = 0;
	unsigned short Occupancy;
	Block* IndexBlock;
	//ѭ��ֱ������Ҷ�ڵ�
	while (1) {
		unsigned int TargetBlock = Track[TrackLength];
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(Occup + TrackLength, IndexBlock->Content + 1, 2);
		Occupancy = Occup[TrackLength];
		//�ж��ǲ���Ҷ�ڵ�
		if (IndexBlock->Content[0] == 1) //��Ҷ�ڵ�
			break;
		void* Target;
		if (index.Type == SQL_INT)
			Target = new int;
		else if (index.Type == SQL_FLOAT)
			Target = new float;
		else
			Target = new string;
		unsigned short P = -1;
		for (unsigned short i = 0; i <= Occupancy - 2; i++) {//��Ҷ���ռ����ΪOccupancy, ��ָ�ֵ��Occupancy-1��, �ֱ�λ��IndexBlock->Content[9+i*(AttrSize+4)], i=0,1,...,Occupancy-2
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[4096];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Value, Target, index.Type) == -1) {//�ҵ���һ��Target, �ϸ����Value
				P = i;
				break;
			}
		}
		delete Target;
		if (P == 65535)//����ý��������ֵ����Value, ֱ��ȡ���һ��ָ��. ����, ȡ��P��ָ��
			P = Occupancy - 1;
		Desce[TrackLength] = P;
		TrackLength++;
		memcpy(Track + TrackLength, IndexBlock->Content + 5 + P * (AttrSize + 4), 4);
	}
	//����Ҷ�ڵ�, ��ʼִ�в������
	if (Occupancy < FanOut - 1) {//���ռ���ʻ�û�ﵽFanOut - 1, ֱ�Ӳ���
		void* Target;
		if (index.Type == SQL_INT)
			Target = new int;
		else if (index.Type == SQL_FLOAT)
			Target = new float;
		else
			Target = new string;
		bool Stored = false;
		for (int i = Occupancy - 1; i >= 0; i--) {
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[1024];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Target, Value, index.Type) == 1)//���������ֵ>�������ֵ, �������
				memcpy(IndexBlock->Content + 5 + (i + 1) * (AttrSize + 4), IndexBlock->Content + 5 + i * (AttrSize + 4), AttrSize + 4);
			else {//���������ֵ<=�������ֵ, �Ͱ���ֵ���ں���
				Stored = true;
				memcpy(IndexBlock->Content + 5 + (i + 1) * (AttrSize + 4), &(position.BlockOffset), 2);
				memcpy(IndexBlock->Content + 7 + (i + 1) * (AttrSize + 4), &(position.ByteOffset), 2);
				if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
					memcpy(IndexBlock->Content + 9 + (i + 1) * (AttrSize + 4), Value, AttrSize);
				else
					memcpy(IndexBlock->Content + 9 + (i + 1) * (AttrSize + 4), ((string*)Value)->c_str(), AttrSize);
				break;
			}
		}
		Occupancy++;
		memcpy(IndexBlock->Content + 1, &Occupancy, 2);
		bm.VisitBlock(IndexBlock, 1);
		delete Target;
		//���Stored=false, ˵����ֵӦ�ò��ڵ�0��λ��, ����Ҫ���ϸ��½��
		if (!Stored) {
			memcpy(IndexBlock->Content + 5, &(position.BlockOffset), 2);
			memcpy(IndexBlock->Content + 7, &(position.ByteOffset), 2);
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(IndexBlock->Content + 9, Value, AttrSize);
			else
				memcpy(IndexBlock->Content + 9, ((string*)Value)->c_str(), AttrSize);
			for (TrackLength--; TrackLength >= 0; TrackLength--) {
				unsigned short P = Desce[TrackLength];
				if (P) {
					IndexBlock = bm.GetBlock(IndexFile, Track[TrackLength]);
					if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
						memcpy(IndexBlock->Content + 5 + P * (AttrSize + 4) - AttrSize, Value, AttrSize);
					else
						memcpy(IndexBlock->Content + 5 + P * (AttrSize + 4) - AttrSize, ((string*)Value)->c_str(), AttrSize);
					bm.VisitBlock(IndexBlock, 1);
					break;
				}
			}
		}
		return;
	}
	//����, ռ����=FanOut - 1, ����
	bool SplitFlag = true;
	Block* NewBlock = GetNewBlock(index);
	NewBlock->Content[0] = 1;
	Sibli[TrackLength] = NewBlock->GetOffset();
	//����Ҷ�ڵ�ָ��
	memcpy(NewBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), 4);
	unsigned int Count = NewBlock->GetOffset();
	memcpy(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), &Count, 4);
	//����ռ����
	Occupancy = FanOut / 2;
	memcpy(IndexBlock->Content + 1, &Occupancy, 2);
	Occupancy = FanOut - FanOut / 2;
	memcpy(NewBlock->Content + 1, &Occupancy, 2);
	//����FanOut - FanOut / 2��Ԫ��
	void* Target;
	if (index.Type == SQL_INT)
		Target = new int;
	else if (index.Type == SQL_FLOAT)
		Target = new float;
	else
		Target = new string;
	int i = FanOut - 2, j = FanOut - FanOut / 2 - 1;//ԭ��Ҷ�ڵ��еĵ�i��Ԫ�ؿ�������Ҷ�ڵ�ĵ�j��Ԫ��λ����
	bool Moved = false;
	while (j >= 0) {
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		if (Compare(Value, Target, index.Type) == -1 || Moved) {//Value < Target
			memcpy(NewBlock->Content + 5 + j * (AttrSize + 4), IndexBlock->Content + 5 + i * (AttrSize + 4), AttrSize + 4);
			j--;
			i--;
		}
		else {//Value > Target
			Moved = true;
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(NewBlock->Content + 9 + j * (AttrSize + 4), Value, AttrSize);
			else
				memcpy(NewBlock->Content + 9 + j * (AttrSize + 4), ((string*)Value)->c_str(), AttrSize);
			memcpy(NewBlock->Content + 5 + j * (AttrSize + 4), &(position.BlockOffset), 2);
			memcpy(NewBlock->Content + 7 + j * (AttrSize + 4), &(position.ByteOffset), 2);
			j--;
		}
	}
	while (!Moved && i >= 0) {//Value��δ������, ˵��Ҫ�浽ԭ���ľ�Ҷ�ڵ���
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		if (Compare(Value, Target, index.Type) == -1) {//Value < Target
			memcpy(IndexBlock->Content + 5 + (i + 1) * (AttrSize + 4), IndexBlock->Content + 5 + i * (AttrSize + 4), AttrSize + 4);
			i--;
		}
		else {//Value > Target
			Moved = true;
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(IndexBlock->Content + 9 + (i + 1) * (AttrSize + 4), Value, AttrSize);
			else
				memcpy(IndexBlock->Content + 9 + (i + 1) * (AttrSize + 4), ((string*)Value)->c_str(), AttrSize);
			memcpy(IndexBlock->Content + 5 + (i + 1) * (AttrSize + 4), &(position.BlockOffset), 2);
			memcpy(IndexBlock->Content + 7 + (i + 1) * (AttrSize + 4), &(position.ByteOffset), 2);
		}
	}
	bm.VisitBlock(IndexBlock, 1);
	bm.VisitBlock(NewBlock, 1);
	if (i == -1) {//Value��δ������, ˵��Ҫ�浽ԭ���ľ�Ҷ�ڵ��ڵĵ�0��λ����, ����Ҫ���ϸ��½��
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(IndexBlock->Content + 9, Value, AttrSize);
		else
			memcpy(IndexBlock->Content + 9, ((string*)Value)->c_str(), AttrSize);
		memcpy(IndexBlock->Content + 5, &(position.BlockOffset), 2);
		memcpy(IndexBlock->Content + 7, &(position.ByteOffset), 2);
		for (int j = TrackLength - 1; j >= 0; j--) {
			unsigned short P = Desce[j];
			if (P) {
				IndexBlock = bm.GetBlock(IndexFile, Track[TrackLength]);
				if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
					memcpy(IndexBlock->Content + 5 + P * (AttrSize + 4) - AttrSize, Value, AttrSize);
				else
					memcpy(IndexBlock->Content + 5 + P * (AttrSize + 4) - AttrSize, ((string*)Value)->c_str(), AttrSize);
				bm.VisitBlock(IndexBlock, 1);
				break;
			}
		}
	}
	//ȡ��NewBlock��С��Ԫ��, ��Ϊ���ڵ���·ָ���
	delete Target;
	Target = new char[AttrSize];
	memcpy(Target, NewBlock->Content + 9, AttrSize);
	//��ʼ��Ҷ�ڵ㷵�ص����ڵ�, �ж��Ƿ���Ҫ����
	bool RootSplitted = false;
	if (TrackLength == 0) {//���ڵ���Ҷ�ڵ�, ���ҽ����˷���. ֱ�������������ѭ��, ���и��ڵ������ز���
		RootSplitted = true;
		SplitFlag = false;
	}
	while (SplitFlag) {
		TrackLength--;
		//IndexBlock�ĵ�P���ӽ�����(��0��ʼ���), ���ѳ������ӽ��Ŀ���ΪNewSon. IndexBlock��ռ����ΪOccupancy
		IndexBlock = bm.GetBlock(IndexFile, Track[TrackLength]);
		unsigned short P = Desce[TrackLength];
		unsigned int NewSon = Sibli[TrackLength + 1];
		Occupancy = Occup[TrackLength];
		if (Occupancy <= FanOut - 1) {//ռ���ʡ�FanOut - 1, ֱ������
			SplitFlag = false;
			for (int i = Occupancy; i >= P + 2; i--)
				memcpy(IndexBlock->Content + 5 + i * (AttrSize + 4) - AttrSize, IndexBlock->Content + 5 + (i - 1) * (AttrSize + 4) - AttrSize, AttrSize + 4);
			memcpy(IndexBlock->Content + 5 + (P + 1) * (AttrSize + 4) - AttrSize, Target, AttrSize);
			memcpy(IndexBlock->Content + 5 + (P + 1) * (AttrSize + 4), &NewSon, 4);
			Occupancy++;
			memcpy(IndexBlock->Content + 1, &Occupancy, 2);
			bm.VisitBlock(IndexBlock, 1);
		}
		else {
			if (IndexBlock->GetOffset() == 0) {//���ڵ����, ������Ҫ��һ������
				SplitFlag = false;
				RootSplitted = true;
			}
			//ȡ���¿�
			NewBlock = GetNewBlock(index);
			NewBlock->Content[0] = 0;
			Sibli[TrackLength] = NewBlock->GetOffset();
			//����ռ����
			Occupancy = (FanOut + 1) / 2;
			memcpy(IndexBlock->Content + 1, &Occupancy, 2);
			Occupancy = FanOut + 1 - (FanOut + 1) / 2;
			memcpy(NewBlock->Content + 1, &Occupancy, 2);
			//�ƶ�������
			if (P <= (FanOut + 1) / 2 - 2) {//˵���������Ǹ�������Ȼ���ھɵĽ����, �ɵĽ��ĵ�FanOut-1~(FanOut+1)/2-1�������Ƶ��µĽ����
				memcpy(NewBlock->Content + 5, IndexBlock->Content + 5 + ((FanOut + 1) / 2 - 1) * (AttrSize + 4), (FanOut + 1 - (FanOut + 1) / 2) * (AttrSize + 4) - AttrSize);
				char* Temp = new char[AttrSize];
				memcpy(Temp, IndexBlock->Content + 5 + ((FanOut + 1) / 2 - 1) * (AttrSize + 4) - AttrSize, AttrSize);
				for (int i = (FanOut + 1) / 2 - 1; i >= P + 2; i--)
					memcpy(IndexBlock->Content + 5 + i * (AttrSize + 4) - AttrSize, IndexBlock->Content + 5 + (i - 1) * (AttrSize + 4) - AttrSize, AttrSize + 4);
				memcpy(IndexBlock->Content + 5 + (P + 1) * (AttrSize + 4) - AttrSize, Target, AttrSize);
				memcpy(IndexBlock->Content + 5 + (P + 1) * (AttrSize + 4), &NewSon, 4);
				delete[]Target;
				Target = Temp;
			}
			else if (P == (FanOut + 1) / 2 - 1) {//˵���������Ǹ����Է����µĽ����, ����Ϊ��0��ָ�����. �ɵĽ��ĵ�FanOut-1~(FanOut+1)/2�������Ƶ��µĽ����
				memcpy(NewBlock->Content + 5 + 4, IndexBlock->Content + 5 + (P + 1) * (AttrSize + 4) - AttrSize, (FanOut - (FanOut + 1) / 2) * (AttrSize + 4));
				memcpy(NewBlock->Content + 5, &NewSon, 4);
			}
			else {//˵���������Ǹ����Է����µĽ����, ����Ϊ��P-(FanOut+1)/2+1��ָ�����. �ɵĽ��ĵ�FanOut-1~(FanOut+1)/2�������Ƶ��µĽ����
				//�ȰѾɵĽ��ĵ�P+1��FanOut-1�����Կ������½��ĵ�P+2-(FanOut+1)/2 ~ FanOut-(FanOut+1)/2������
				//�ٰѷ��ѵĽ�㿽�����½��ĵ�P+1-(FanOut+1)/2�����Դ�
				//�ٰѾɵĽ��ĵ�(FanOut+1)/2��P�����Կ������½��ĵ�0 ~ P-(FanOut+1)/2������
				//Target�洢�ɵĽ��ĵ�(FanOut+1)/2��ָ��ǰ�������
				memcpy(NewBlock->Content + 5 + (P + 2 - (FanOut + 1) / 2) * (AttrSize + 4) - AttrSize, IndexBlock->Content + 5 + (P + 1) * (AttrSize + 4) - AttrSize, (FanOut - P - 1) * (AttrSize + 4));
				memcpy(NewBlock->Content + 5 + (P + 1 - (FanOut + 1) / 2) * (AttrSize + 4) - AttrSize, Target, AttrSize);
				memcpy(NewBlock->Content + 5 + (P + 1 - (FanOut + 1) / 2) * (AttrSize + 4), &NewSon, 4);
				memcpy(NewBlock->Content + 5, IndexBlock->Content + 5 + ((FanOut + 1) / 2) * (AttrSize + 4), (P + 1 - (FanOut + 1) / 2) * (AttrSize + 4) - AttrSize);
				memcpy(Target, IndexBlock->Content + 5 + ((FanOut + 1) / 2) * (AttrSize + 4) - AttrSize, AttrSize);
			}
			bm.VisitBlock(IndexBlock, 1);
			bm.VisitBlock(NewBlock, 1);
		}
	}
	if (RootSplitted) {//������ڵ�����˷���
		NewBlock = GetNewBlock(index);
		IndexBlock = bm.GetBlock(IndexFile, 0);
		memcpy(NewBlock->Content, IndexBlock->Content, BlockSize);
		IndexBlock->Content[0] = 0;
		unsigned short temp1;
		temp1 = 2;
		memcpy(IndexBlock->Content + 1, &temp1, 2);
		unsigned int temp2 = NewBlock->GetOffset();
		memcpy(IndexBlock->Content + 5, &temp2, 4);
		memcpy(IndexBlock->Content + 9, Target, AttrSize);
		temp2 = Sibli[0];
		memcpy(IndexBlock->Content + 9 + AttrSize, &temp2, 4);
		bm.VisitBlock(IndexBlock, 1);
		bm.VisitBlock(NewBlock, 1);
	}
	delete[]Target;
}


//��B+����ɾ��list����Ӧ��һϵ�н��
void IndexManager::DeleteTuples(const IndexSchema& index, DeletedTuple* list) {
	DeletedTuple* lst = list;
	while (lst) {
		Delete(index, lst);
		lst = lst->Next;
	}
}

//��B+����ɾ���������
void IndexManager::Delete(const IndexSchema& index, DeletedTuple* node) {
	const void* Value = node->values[index.AttributeNumber];
	//��ʼ������
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	unsigned int AttrSize = index.AttrSize();
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	//��ʼ����
	//B+�������ж���, ����Index������2^16-1=65535��, �����������15, �ظ��ڵ㵽Ҷ�ڵ�����16����
	unsigned int Track[16] = { 0 };//�Ӹ��ڵ�һֱ׷�ٵ�Ҷ�ڵ�, ��·�Ŀ��
	unsigned short Occup[16] = { 0 };//��·ÿ�����ռ����
	unsigned short Desce[16] = { 0 };//��·��ÿ����ֱ��صڼ���ָ��������
	int TrackLength = 0;
	unsigned short Occupancy;
	unsigned int TargetBlock;
	Block* IndexBlock;
	//ѭ��ֱ������Ҷ�ڵ�
	while (1) {
		TargetBlock = Track[TrackLength];
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(Occup + TrackLength, IndexBlock->Content + 1, 2);
		Occupancy = Occup[TrackLength];
		//�ж��ǲ���Ҷ�ڵ�
		if (IndexBlock->Content[0] == 1) //��Ҷ�ڵ�
			break;
		void* Target;
		if (index.Type == SQL_INT)
			Target = new int;
		else if (index.Type == SQL_FLOAT)
			Target = new float;
		else
			Target = new string;
		unsigned short P = -1;
		for (unsigned short i = 0; i <= Occupancy - 2; i++) {//��Ҷ���ռ����ΪOccupancy, ��ָ�ֵ��Occupancy-1��, �ֱ�λ��IndexBlock->Content[9+i*(AttrSize+4)], i=0,1,...,Occupancy-2
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[4096];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Value, Target, index.Type) == -1) {//�ҵ���һ��Target, �ϸ����Value
				P = i;
				break;
			}
		}
		delete Target;
		if (P == 65535)//����ý��������ֵ����Value, ֱ��ȡ���һ��ָ��. ����, ȡ��P��ָ��
			P = Occupancy - 1;
		Desce[TrackLength] = P;
		TrackLength++;
		memcpy(Track + TrackLength, IndexBlock->Content + 5 + P * (AttrSize + 4), 4);
	}
	//����Ҷ�ڵ�, ��ʼִ��ɾ������
	void* Target;
	if (index.Type == SQL_INT)
		Target = new int;
	else if (index.Type == SQL_FLOAT)
		Target = new float;
	else
		Target = new string;
	unsigned short DeletedP = -1;//��¼ɾ����λ��, ��0~Occupancy-1
	for (int i = 0; ; i++) {
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		if (Compare(Target, Value, index.Type) == 0) {   //���, ����DeletedP��ֵ, ����ѭ��
			DeletedP = i;
			break;
		}
	}
	//��DeletedP�����Ԫ������ǰ��
	for (int i = DeletedP + 1; i <= Occupancy - 1; i++) 
		memcpy(IndexBlock->Content + 5 + (i - 1) * (AttrSize + 4), IndexBlock->Content + 5 + i * (AttrSize + 4), AttrSize + 4);
	Occupancy = --Occup[TrackLength];
	memcpy(IndexBlock->Content + 1, &Occupancy, 2);//ռ����--
	bm.VisitBlock(IndexBlock, 1);
	//���DeletedP=0, ˵��ɾ����ֵ�ڵ�0��λ��, Ҫ���ϸ��½��
	if (!DeletedP) {
		//�洢��Ҷ�ڵ��µ���Сֵ
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9, AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9, AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		for (int i = TrackLength - 1; i >= 0; i--)
			if (Desce[i]) {
				IndexBlock = bm.GetBlock(IndexFile, Track[i]);
				if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
					memcpy(IndexBlock->Content + 5 + Desce[i] * (AttrSize + 4) - AttrSize, Target, AttrSize);
				else
					memcpy(IndexBlock->Content + 5 + Desce[i] * (AttrSize + 4) - AttrSize, ((string*)Target)->c_str(), AttrSize);
				bm.VisitBlock(IndexBlock, 1);
				break;
			}
	}
	delete Target;
	//��ʼ�ϲ����
	if (!TrackLength)//TrackLength=0, ˵�����ڵ����Ҷ�ڵ�, �����κβ���
		return;
	int MergeDir = 0;//-1:����ߵ��ֵܽ��ϲ� 1:���ұߵ��ֵܽ��ϲ�
	while (1) {
		Occupancy = Occup[TrackLength];
		TargetBlock = Track[TrackLength];
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		unsigned short Leaf = (IndexBlock->Content[0]) ? 1 : 0;
		if (TrackLength == 0 || Occupancy >= (FanOut + 1 - Leaf) / 2)//�����ǰ�Ǹ��ڵ�, ����ռ���ʡ�(FanOut-Leaf)/2��ȡ��, ����Ҫ�ϲ�
			break;
		//��ǰ�����Block[FatherOffset]�ĵ�P���ӽ��
		unsigned int FatherOffset = Track[TrackLength - 1];
		unsigned short P = Desce[TrackLength - 1];
		if (P < Occup[TrackLength - 1] - 1)
			MergeDir = 1;//���ұߵ��ֵܽ��ϲ�
		else
			MergeDir = -1;//����ߵ��ֵܽ��ϲ�
		Block* FatherBlock = bm.GetBlock(IndexFile, FatherOffset);
		//SiblOffset���ֵܽ��Ŀ�ƫ����
		unsigned short SiblOffset;
		memcpy(&SiblOffset, FatherBlock->Content + 5 + (P + MergeDir) * (AttrSize + 4), 4);
		//SiblBlock���ֵܽ��Ŀ�ָ��
		Block* SiblBlock = bm.GetBlock(IndexFile, SiblOffset);
		//SiblOccupancy���ֵܽ���ռ����
		unsigned short SiblOccupancy;
		memcpy(&SiblOccupancy, SiblBlock->Content + 1, 2);
		//Ϊ�˷����������, ����ͳһ��IndexBlock�������, SiblBlock�����Ҳ�, P��ӦIndexBlock��ָ����
		if (MergeDir == -1) {
			Block* tempBlock = IndexBlock; IndexBlock = SiblBlock; SiblBlock = tempBlock;
			P--;
			unsigned short tempOcc = Occupancy; Occupancy = SiblOccupancy; SiblOccupancy = tempOcc;
		}
		//�ж��Ƿ���Ҫ�ϲ�
		if (Occupancy + SiblOccupancy > FanOut - Leaf) {//����Ҫ�ϲ�, ֻ��Ҫ���ֵܽ������һ�������ݼ���. �����¸����
			unsigned short Occ1 = (Occupancy + SiblOccupancy) / 2, Occ2 = Occupancy + SiblOccupancy - (Occupancy + SiblOccupancy) / 2;
			if (Leaf) {//Ҷ�ڵ�Ĵ���ʽ�ͷ�Ҷ��㲻ͬ
				char* Buffer = new char[(Occupancy + SiblOccupancy) * (AttrSize + 4)];
				memcpy(Buffer, IndexBlock->Content + 5, Occupancy* (AttrSize + 4));
				memcpy(Buffer + Occupancy * (AttrSize + 4), SiblBlock->Content + 5, SiblOccupancy* (AttrSize + 4));

				memcpy(IndexBlock->Content + 5, Buffer, Occ1* (AttrSize + 4));
				memcpy(SiblBlock->Content + 5, Buffer + Occ1 * (AttrSize + 4), Occ2* (AttrSize + 4));

				memcpy(IndexBlock->Content + 1, &Occ1, 2);
				memcpy(SiblBlock->Content + 1, &Occ2, 2);

				memcpy(FatherBlock->Content + 9 + P * (AttrSize + 4), SiblBlock->Content + 9, AttrSize);
				delete[]Buffer;
			}
			else {
				char* Buffer = new char[(Occupancy + SiblOccupancy) * (AttrSize + 4) - AttrSize];
				memcpy(Buffer, IndexBlock->Content + 5, Occupancy * (AttrSize + 4) - AttrSize);
				memcpy(Buffer + Occupancy * (AttrSize + 4) - AttrSize, FatherBlock->Content + 9 + P * (AttrSize + 4), AttrSize);
				memcpy(Buffer + Occupancy * (AttrSize + 4), SiblBlock->Content + 5, SiblOccupancy * (AttrSize + 4) - AttrSize);

				memcpy(IndexBlock->Content + 5, Buffer, Occ1 * (AttrSize + 4) - AttrSize);
				memcpy(SiblBlock->Content + 5, Buffer + Occ1 * (AttrSize + 4), Occ2 * (AttrSize + 4) - AttrSize);

				memcpy(IndexBlock->Content + 1, &Occ1, 2);
				memcpy(SiblBlock->Content + 1, &Occ2, 2);

				memcpy(FatherBlock->Content + 9 + P * (AttrSize + 4), Buffer + Occ1 * (AttrSize + 4) - AttrSize, AttrSize);
				delete[]Buffer;
			}
			//����д��־
			bm.VisitBlock(IndexBlock, 1);
			bm.VisitBlock(SiblBlock, 1);
			bm.VisitBlock(FatherBlock, 1);
			break;
		}
		else {//��Ҫ���ֵܽ��ϲ�. �����¸����
			unsigned short Occ = Occupancy + SiblOccupancy;
			if (Leaf) {//Ҷ�ڵ�Ĵ���ʽ�ͷ�Ҷ��㲻ͬ
				memcpy(IndexBlock->Content + 5 + Occupancy * (AttrSize + 4), SiblBlock->Content + 5, SiblOccupancy * (AttrSize + 4));
				memcpy(IndexBlock->Content + 1, &Occ, 2);
				memcpy(IndexBlock->Content + 5 +(FanOut - 1) * (AttrSize + 4), SiblBlock->Content + 5 + (FanOut - 1) * (AttrSize + 4), 4);
				DeleteBlock(index, SiblBlock);
			}
			else {
				memcpy(IndexBlock->Content + 5 + Occupancy * (AttrSize + 4), SiblBlock->Content + 5, SiblOccupancy* (AttrSize + 4) - AttrSize);
				memcpy(IndexBlock->Content + 5 + Occupancy * (AttrSize + 4) - AttrSize, FatherBlock->Content + 9 + P * (AttrSize + 4), AttrSize);
				memcpy(IndexBlock->Content + 1, &Occ, 2);
				DeleteBlock(index, SiblBlock);
			}
			//�����ڵ��P+1��ָ��(����ǰ��ķָ�ֵ)ɾ��, ����ȫ��ǰ��
			for (int i = P + 1; i <= Occup[TrackLength - 1] - 2; i++)
				memcpy(FatherBlock->Content + 5 + i * (AttrSize + 4) - AttrSize, FatherBlock->Content + 5 + (i + 1) * (AttrSize + 4) - AttrSize, AttrSize + 4);
			//���ڵ�ռ����--
			Occup[TrackLength - 1]--;
			memcpy(FatherBlock->Content + 1, Occup + TrackLength - 1, 2);
			//����д��־
			bm.VisitBlock(IndexBlock, 1);
			bm.VisitBlock(FatherBlock, 1);
		}
		TrackLength--;
	}
	if (TrackLength == 0 && Occupancy == 1) {//���ڵ�ռ����ֻ��1, ɾ���ø��ڵ�
		memcpy(&TargetBlock, IndexBlock->Content + 5, 4);
		Block* FreeBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(IndexBlock->Content, FreeBlock->Content, 3);
		memcpy(IndexBlock->Content + 5, FreeBlock->Content + 5, BlockSize - 5);
		DeleteBlock(index, FreeBlock);
	}
}

//�ӿ����б��еõ����п�
Block* IndexManager::GetNewBlock(const IndexSchema& index) {
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	Block* IndexHead = bm.GetBlock(IndexFile, 0);
	unsigned short ListPtr;
	memcpy(&ListPtr, IndexHead->Content + 3, 2);
	if (ListPtr == 0)
		return bm.AppendBlock(IndexFile);
	else {
		Block* FreeBlock = bm.GetBlock(IndexFile, ListPtr);
		memcpy(IndexHead->Content + 3, FreeBlock->Content + 3, 2);
		bm.VisitBlock(IndexHead, 1);
		return FreeBlock;
	}
}

//�����п��������б�
void IndexManager::DeleteBlock(const IndexSchema& index, Block* Obj) {
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	Block* IndexHead = bm.GetBlock(IndexFile, 0);
	memcpy(Obj->Content + 3, IndexHead->Content + 3, 2);
	unsigned short temp = Obj->GetOffset();
	memcpy(IndexHead->Content + 3, &temp, 2);
	bm.VisitBlock(Obj, 1);
	bm.VisitBlock(IndexHead, 1);
}