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


//创建索引, 根据table中已有的内容构建B+树
//B+树每个结点的结构:
// 1 Byte: 指示当前是否是叶节点.
// 2 Bytes: 指示当前块占用率. 如果是叶节点, 则代表元组个数, 至多FanOut-1; 如果是非叶结点, 则代表子结点个数, 至多FanOut.
// 2 Bytes: 空闲列表指针. 当且仅当目前是第0块、或占用率=0时, 这个指针才有意义. 
// FanOut*4 Bytes: FanOut个指针. 如果是叶节点, 则每个指针=(块偏移量, 字节偏移量)指向元组位置;
//								 如果是非叶结点, 则每个指针=块偏移量, 指向下一个结点的块.
// (FanOut-1)*m Bytes: (FanOut-1)个元素值, 其中m=AttrSize
void IndexManager::CreateIndex(const TableSchema& table, const IndexSchema& index) {
	//初始化数据
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	string TableFile = DataDirectory + index.TableName + ".Table";
	unsigned int AttrSize = index.AttrSize();
	unsigned int AttrOffset = table.AttrOffset(index.AttributeNumber);
	unsigned int RecordSize = table.RecordSize();
	unsigned int RecordNum = BlockSize / RecordSize;
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	unsigned int TableSize = FileSize(TableFile);
	//遍历表数据文件
	unsigned short RecordCnt = 0;
	TempRecord lst(0, 0, 0);//哑头指针
	TempRecord* ptr = &lst;
	for (unsigned int i = 0; i < TableSize; i++) {
		//取得第i块元组数据
		Block* TableBlock = bm.GetBlock(TableFile, i);
		for (unsigned int j = 0; j < RecordNum; j++) {
			//若当前元组为空, 跳过
			if (TableBlock->Content[j * RecordSize] == 0)
				continue;
			//否则, 将当前元组的位置、属性值存入叶节点
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
	//如果是空表
	if (RecordCnt == 0) {
		//创建空文件
		Block* IndexBlock = bm.AppendBlock(IndexFile);
		//修改文件内容
		IndexBlock->Content[0] = 1;//叶节点
		IndexBlock->Content[1] = IndexBlock->Content[2] = 0;//占用率为0
		IndexBlock->Content[3] = IndexBlock->Content[4] = 0;//空闲列表指针=NULL
		//设置写标记
		bm.VisitBlock(IndexBlock, 1);
		return;
	}
	//对链表排序
	TempRecord** Arr = new TempRecord * [RecordCnt];
	ptr = lst.Next;
	for (int i = 0; i < RecordCnt; i++) {
		Arr[i] = ptr;
		ptr = ptr->Next;
	}
	QuickSort(Arr, 0, RecordCnt - 1, index.Type);
	/*cout << "排序结果:" << endl;
	for (int i = 0; i < RecordCnt; i++) {
		cout << "Value:" << ((string*)(Arr[i]->Value))->c_str() << " BlockOffset: " << Arr[i]->BlockOffset << " ByteOffset: " << Arr[i]->ByteOffset << endl;
	}*/
	//现在, 整个表的元组全都在Arr中, 个数为RecordCnt
	if (RecordCnt <= FanOut - 1) {//能直接存入一个叶节点
		//创建空文件
		Block* IndexBlock = bm.AppendBlock(IndexFile);
		//修改文件内容
		IndexBlock->Content[0] = 1;//叶节点
		memcpy(IndexBlock->Content + 1, &RecordCnt, 2);//占用率为RecordCnt
		IndexBlock->Content[3] = IndexBlock->Content[4] = 0;//空闲列表指针=NULL
		//存储Arr中的所有信息
		for (int i = 0; i < RecordCnt; i++) {
			memcpy(IndexBlock->Content + 5 + i * (4 + AttrSize), &(Arr[i]->BlockOffset), 2);
			memcpy(IndexBlock->Content + 7 + i * (4 + AttrSize), &(Arr[i]->ByteOffset), 2);
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(IndexBlock->Content + 9 + i * (4 + AttrSize), Arr[i]->Value, AttrSize);
			else
				memcpy(IndexBlock->Content + 9 + i * (4 + AttrSize), ((string*)Arr[i]->Value)->c_str(), AttrSize);
		}
		//设置剩下的空间
		if (RecordCnt < FanOut - 1)
			memset(IndexBlock->Content + 5 + RecordCnt * (4 + AttrSize), 0, (4 + AttrSize) * (FanOut - 1 - RecordCnt));
		//设置末尾指针
		memset(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), 0, 4);
		//设置写标记
		bm.VisitBlock(IndexBlock, 1);
		//释放Arr及链表的内存空间
		for (int i = 0; i < RecordCnt; i++)
			delete Arr[i];
		delete[] Arr;
	}
	else {//一个叶节点存不下所有记录
		unsigned int LevelStart = 1;//B+树当前层的起始块
		unsigned int LevelCount = 0;//B+树当前层有多少块
		unsigned short Occupancy = FanOut - 1;
		unsigned short Expected = FanOut - 1;
		Block* IndexBlock = bm.AppendBlock(IndexFile);//第0块, 暂时不管
		for (unsigned int i = 0; i < RecordCnt; i++) {//逐个填满叶节点, 一共要占用RecordCnt / (FanOut - 1)上取整个叶节点
			if (Occupancy == Expected) {//上一块已满, 要开辟下一块的存储空间
				LevelCount++;
				Occupancy = 0;
				IndexBlock = bm.AppendBlock(IndexFile);//当前是第LevelStart + LevelCount - 1块
				//设置叶节点标志
				IndexBlock->Content[0] = 1;
				//设置占用率和末尾指针
				if (RecordCnt - i <= FanOut - 1) {//还剩RecordCnt - i个记录, 小于等于FanOut - 1, 说明这是最后一个叶节点
					Expected = RecordCnt - i;
					memcpy(IndexBlock->Content + 1, &Expected, 2);
					memset(IndexBlock->Content + 3, 0, 2);
					memset(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), 0, 4);
				}
				else if (RecordCnt - i < (FanOut - 1) + FanOut / 2){//还剩RecordCnt - i个记录, 大于(FanOut - 1), 但小于(FanOut - 1)+(FanOut - 1)/2上取整, 说明剩下来要拆成2个叶节点
					Expected = (RecordCnt - i) / 2;
					memcpy(IndexBlock->Content + 1, &Expected, 2);
					memset(IndexBlock->Content + 3, 0, 2);
					unsigned int temp1 = LevelStart + LevelCount;
					memcpy(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), &temp1, 4);
				}
				else {//还剩RecordCnt - i个记录, 大于等于(FanOut - 1)+(FanOut - 1)/2上取整, 接下来的那个叶节点直接塞满
					Expected = FanOut - 1;
					memcpy(IndexBlock->Content + 1, &Expected, 2);
					memset(IndexBlock->Content + 3, 0, 2);
					unsigned int temp1 = LevelStart + LevelCount;
					memcpy(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), &temp1, 4);
				}
				//设置写标记
				bm.VisitBlock(IndexBlock, 1);
			}
			//将当前元组存入当前块
			memcpy(IndexBlock->Content + 5 + Occupancy * (4 + AttrSize), &(Arr[i]->BlockOffset), 2);
			memcpy(IndexBlock->Content + 7 + Occupancy * (4 + AttrSize), &(Arr[i]->ByteOffset), 2);
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(IndexBlock->Content + 9 + Occupancy * (4 + AttrSize), Arr[i]->Value, AttrSize);
			else
				memcpy(IndexBlock->Content + 9 + Occupancy * (4 + AttrSize), ((string*)Arr[i]->Value)->c_str(), AttrSize);
			//清除链表无用元素
			if (Occupancy) {//只有当前块的第0个记录的信息才需要保留, 其余释放
				delete Arr[i];
				Arr[i] = 0;
			}
			Occupancy++;
		}
		unsigned int LastLevelCount = RecordCnt;
		while (LevelCount > 1) {//层序构建, 直到LevelCount = 1, 说明当前层只剩一个结点, 也就是根节点
			//先把上一次剩下的LevelCount个链表结点保存到数组
			for (unsigned int i = 0, j = 0; i < LastLevelCount; i++)
				if (Arr[i]) {
					TempRecord* temp = Arr[i];
					Arr[i] = 0;
					Arr[j] = temp;
					j++;
				}
			//开始构建
			unsigned int NewLevelStart = LevelStart + LevelCount;
			unsigned int NewLevelCount = 0;
			unsigned short Occupancy = FanOut;
			unsigned short Expected = FanOut;
			for (unsigned int i = 0; i < LevelCount; i++) {
				if (Occupancy == Expected) {//上一块已满, 要开辟下一块的存储空间
					NewLevelCount++;
					Occupancy = 0;
					//若LevelCount > FanOut, 则当前是第NewLevelStart + NewLevelCount - 1块; 否则直接取第0块作为根节点
					IndexBlock = (LevelCount > FanOut) ? bm.AppendBlock(IndexFile) : bm.GetBlock(IndexFile, 0);
					//设置非叶节点标志
					IndexBlock->Content[0] = 0;
					//设置占用率
					if (LevelCount - i <= FanOut) {//还剩LevelCount - i个记录, 小于等于FanOut, 说明这是当前层最后一个节点
						Expected = LevelCount - i;
						memcpy(IndexBlock->Content + 1, &Expected, 2);
						memset(IndexBlock->Content + 3, 0, 2);
					}
					else if (LevelCount - i < FanOut + (FanOut + 1) / 2) {//还剩LevelCount - i个记录, 大于FanOut, 但小于FanOut+FanOut/2上取整, 说明剩下来要拆成两个结点
						Expected = (LevelCount - i) / 2;
						memcpy(IndexBlock->Content + 1, &Expected, 2);
						memset(IndexBlock->Content + 3, 0, 2);
					}
					else {//还剩LevelCount - i个记录, 大于等于FanOut+FanOut/2上取整, 接下来的那个叶节点直接塞满
						Expected = FanOut;
						memcpy(IndexBlock->Content + 1, &Expected, 2);
						memset(IndexBlock->Content + 3, 0, 2);
					}
					//设置写标记
					bm.VisitBlock(IndexBlock, 1);
				}
				//将当前元组存入当前块
				unsigned int temp = LevelStart + i;
				memcpy(IndexBlock->Content + 5 + Occupancy * (4 + AttrSize), &temp, 4);
				if (Occupancy) {//非叶结点存的值表示分隔值, 例如第0个值表示第0个子树和第1个子树的分隔值. 所以要从1开始
					if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
						memcpy(IndexBlock->Content + 5 + Occupancy * (4 + AttrSize) - AttrSize, Arr[i]->Value, AttrSize);
					else
						memcpy(IndexBlock->Content + 5 + Occupancy * (4 + AttrSize) - AttrSize, ((string*)Arr[i]->Value)->c_str(), AttrSize);
				}
				//清除链表无用元素
				if (Occupancy) {//只有当前块的第0个记录的信息才需要保留, 其余释放
					delete Arr[i];
					Arr[i] = 0;
				}
				Occupancy++;
			}
			//迭代, 构建下一层
			LastLevelCount = LevelCount;
			LevelCount = NewLevelCount;
			LevelStart = NewLevelStart;
		}
		//释放链表和数组内存
		delete Arr[0];//这个时候必然只剩下一个链表结点
		delete[] Arr;
	}
}


//删除TableName表的所有索引文件
void IndexManager::DropIndexOfTable(const string& TableName) {
	Remove(IndexDirectory + TableName + "/");
}


//删除单个索引
void IndexManager::DropIndex(const string& IndexName) {
	DeleteFiles(IndexDirectory, IndexName + ".Index");
}


//查询, 返回链表. 保证链表元素按(块偏移量, 字节偏移量)排序
Pos* IndexManager::Select(const IndexSchema& index,	int length, int op[], void* value[]) {
	//扫描整个长为length的数组. 根据op, 把＞、≥的最大条件找出来, 把＜、≤的最小条件找出来. 如果有两个不等的＝条件, 直接返回NULL. 把所有≠的条件并入一个数组
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
	//查询优化, 判断＞、≥、＜、≤、＝、≠的条件的交集是否为空, 就能直接返回NULL
	//将＞和≥合并成一个条件
	if (G && GEQ) {
		if (Compare(G, GEQ, index.Type) >= 0)
			GEQ = NULL;//"＞"的下界大于等于"≥"的下界, 以"＞"为准
		else
			G = NULL;//"＞"的下界小于"≥"的下界, 以"≥"为准
	}
	//将＜和≤合并成一个条件
	if (L && LEQ) {
		if (Compare(L, LEQ, index.Type) <= 0)
			LEQ = NULL;//"＜"的下界小于等于"≤"的下界, 以"＜"为准
		else
			L = NULL;//"＜"的下界大于"≤"的下界, 以"≤"为准
	}
	//判断＝和＞, ≥, ＜, ≤是否冲突
	if ((EQ && G && Compare(EQ, G, index.Type) <= 0)
		|| (EQ && GEQ && Compare(EQ, GEQ, index.Type) == -1)
		|| (EQ && L && Compare(EQ, L, index.Type) >= 0)
		|| (EQ && LEQ && Compare(EQ, LEQ, index.Type) == 1)) {
		delete[]NEQ;
		return NULL;
	}
	//判断＝和≠是否冲突
	if (EQ)
		for (int i=0;i<LNEQ;i++)
			if (Compare(EQ, NEQ[i], index.Type) == 0) {
				delete[]NEQ;
				return NULL;
			}
	//准备开始查询, 首先判断查找目标, 确定查询起点Value
	void* Value;
	if (EQ)
		Value = EQ;
	else if (G)
		Value = G;
	else if (GEQ)
		Value = GEQ;
	else
		Value = NULL;//第一个位置的NULL会被识别成无穷小
	//初始化数据
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
	//循环直至遇到叶节点
	while (1) {
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(&Occupancy, IndexBlock->Content + 1, 2);
		//判断是不是叶节点
		if (IndexBlock->Content[0] == 1) //是叶节点
			break;
		unsigned short P = -1;
		for (unsigned short i = 0; i <= Occupancy - 2; i++) {//非叶结点占用率为Occupancy, 则分隔值有Occupancy-1个, 分别位于IndexBlock->Content[9+i*(AttrSize+4)], i=0,1,...,Occupancy-2
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[4096];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Value, Target, index.Type) == -1) {//找到第一个Target, 严格大于Value
				P = i;
				break;
			}
		}
		if (P == 65535)//如果该结点内所有值都≤Value, 直接取最后一个指针. 否则, 取第P个指针
			P = Occupancy - 1;
		memcpy(&TargetBlock, IndexBlock->Content + 5 + P * (AttrSize + 4), 4);
	}
	//遇到叶节点, 开始寻找该值
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
		//对于EQ或GEQ找到第一个≥Value的值, 对于G找到第一个＞Value的值
			if (EQ) {//如果有相等判断条件
				if (Compare(Value, Target, index.Type)) { //如果有相等判断条件, 且这个时候还没找到相等的值, 就说明根本找不到目标值
					delete Target;
					delete[]NEQ;
					return NULL;
				}
				else {//找到目标值
					delete Target;
					delete[]NEQ;
					Pos* ReturnValue = new Pos;
					ReturnValue->Next = NULL;
					memcpy(&(ReturnValue->BlockOffset), IndexBlock->Content + 5 + i * (AttrSize + 4), 2);
					memcpy(&(ReturnValue->ByteOffset), IndexBlock->Content + 7 + i * (AttrSize + 4), 2);
					return ReturnValue;
				}
			}
			P = i;//记下最小的大于等于Value的值的下标
			break;
		}
	}
	if (P == 65535) {//如果该叶节点所有值都小于Value, 说明目标值在下一个叶节点中
		memcpy(&TargetBlock, IndexBlock->Content + 5 + (FanOut - 1) * (AttrSize + 4), 4);
		if (TargetBlock == 0) {//当前已经是最后一个叶节点, 没有下一个叶节点, 返回NULL
			delete Target;
			delete[] NEQ;
			return NULL;
		}
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(&Occupancy, IndexBlock->Content + 1, 2);
		P = 0;
	}
	//现在, IndexBlock的第P个值应该存储了最小的满足"≥Value"(对于GEQ)或"＞Value"(对于G)的值
	//一直往后遍历, 直到"＜L"或"≤LEQ"有一个不满足, 就跳出循环
	int Count = 0;
	Pos ReturnValue; Pos* ptr = &ReturnValue;
	while (1) {
		if (P == Occupancy) {
			memcpy(&TargetBlock, IndexBlock->Content + 5 + (FanOut - 1) * (AttrSize + 4), 4);
			if (TargetBlock == 0) //当前已经是最后一个叶节点, 没有下一个叶节点, break
				break;
			IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
			memcpy(&Occupancy, IndexBlock->Content + 1, 2);
			P = 0;
		}
		//取得IndexBlock第P个值, 存入Target
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + P * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + P * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		//"＜L"或"≤LEQ"有一个不满足, 就跳出循环
		if (L && Compare(Target, L, index.Type) >= 0 || LEQ && Compare(Target, LEQ, index.Type) == 1)
			break;
		//若"≠"条件不满足, 则跳过当前轮
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
	//对结果排序
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


//检查value是否已经在B+树中存在
bool IndexManager::Conflict(const IndexSchema& index, void* value) {
	const void* Value = value;
	//初始化数据
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	unsigned int AttrSize = index.AttrSize();
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	//开始查找
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
	//循环直至遇到叶节点
	while (1) {
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(&Occupancy, IndexBlock->Content + 1, 2);
		//判断是不是叶节点
		if (IndexBlock->Content[0] == 1) //是叶节点
			break;
		unsigned short P = -1;
		for (unsigned short i = 0; i <= Occupancy - 2; i++) {//非叶结点占用率为Occupancy, 则分隔值有Occupancy-1个, 分别位于IndexBlock->Content[9+i*(AttrSize+4)], i=0,1,...,Occupancy-2
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[4096];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Value, Target, index.Type) == -1) {//找到第一个Target, 严格大于Value
				P = i;
				break;
			}
		}
		if (P == 65535)//如果该结点内所有值都≤Value, 直接取最后一个指针. 否则, 取第P个指针
			P = Occupancy - 1;
		memcpy(&TargetBlock, IndexBlock->Content + 5 + P * (AttrSize + 4), 4);
	}
	//遇到叶节点, 开始寻找该值
	for (int i = Occupancy - 1; i >= 0; i--) {
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		if (Compare(Target, Value, index.Type) == 0) {//如果找到
			delete Target;
			return true;
		}
	}
	delete Target;
	return NULL;
}


//在B+树中插入结点
void IndexManager::Insert(const IndexSchema& index, void* values[], const Pos& position) {
	const void* Value = values[index.AttributeNumber];
	//初始化数据
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	unsigned int AttrSize = index.AttrSize();
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	//开始查找
	//B+树至少有二叉, 整个Index至多有2^16-1=65535块, 因此树高至多15, 沿根节点到叶节点至多16个块
	unsigned int Track[16] = { 0 };//从根节点一直追踪到叶节点, 沿路的块号
	unsigned short Occup[16] = { 0 };//沿路每个块的占用率
	unsigned short Desce[16] = { 0 };//沿路的每个块分别沿第几个指针往下走
	unsigned short Sibli[16] = { 0 };//因插入&分裂而产生的兄弟结点
	int TrackLength = 0;
	unsigned short Occupancy;
	Block* IndexBlock;
	//循环直至遇到叶节点
	while (1) {
		unsigned int TargetBlock = Track[TrackLength];
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(Occup + TrackLength, IndexBlock->Content + 1, 2);
		Occupancy = Occup[TrackLength];
		//判断是不是叶节点
		if (IndexBlock->Content[0] == 1) //是叶节点
			break;
		void* Target;
		if (index.Type == SQL_INT)
			Target = new int;
		else if (index.Type == SQL_FLOAT)
			Target = new float;
		else
			Target = new string;
		unsigned short P = -1;
		for (unsigned short i = 0; i <= Occupancy - 2; i++) {//非叶结点占用率为Occupancy, 则分隔值有Occupancy-1个, 分别位于IndexBlock->Content[9+i*(AttrSize+4)], i=0,1,...,Occupancy-2
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[4096];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Value, Target, index.Type) == -1) {//找到第一个Target, 严格大于Value
				P = i;
				break;
			}
		}
		delete Target;
		if (P == 65535)//如果该结点内所有值都≤Value, 直接取最后一个指针. 否则, 取第P个指针
			P = Occupancy - 1;
		Desce[TrackLength] = P;
		TrackLength++;
		memcpy(Track + TrackLength, IndexBlock->Content + 5 + P * (AttrSize + 4), 4);
	}
	//遇到叶节点, 开始执行插入操作
	if (Occupancy < FanOut - 1) {//如果占用率还没达到FanOut - 1, 直接插入
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
			if (Compare(Target, Value, index.Type) == 1)//如果结点里的值>待插入的值, 整体后移
				memcpy(IndexBlock->Content + 5 + (i + 1) * (AttrSize + 4), IndexBlock->Content + 5 + i * (AttrSize + 4), AttrSize + 4);
			else {//如果结点里的值<=待插入的值, 就把新值插在后面
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
		//如果Stored=false, 说明新值应该插在第0个位置, 而且要向上更新结点
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
	//否则, 占用率=FanOut - 1, 分裂
	bool SplitFlag = true;
	Block* NewBlock = GetNewBlock(index);
	NewBlock->Content[0] = 1;
	Sibli[TrackLength] = NewBlock->GetOffset();
	//设置叶节点指针
	memcpy(NewBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), 4);
	unsigned int Count = NewBlock->GetOffset();
	memcpy(IndexBlock->Content + 5 + (FanOut - 1) * (4 + AttrSize), &Count, 4);
	//设置占用率
	Occupancy = FanOut / 2;
	memcpy(IndexBlock->Content + 1, &Occupancy, 2);
	Occupancy = FanOut - FanOut / 2;
	memcpy(NewBlock->Content + 1, &Occupancy, 2);
	//拷贝FanOut - FanOut / 2个元组
	void* Target;
	if (index.Type == SQL_INT)
		Target = new int;
	else if (index.Type == SQL_FLOAT)
		Target = new float;
	else
		Target = new string;
	int i = FanOut - 2, j = FanOut - FanOut / 2 - 1;//原来叶节点中的第i个元素拷贝到新叶节点的第j个元素位置上
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
	while (!Moved && i >= 0) {//Value还未被存入, 说明要存到原来的旧叶节点内
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
	if (i == -1) {//Value还未被存入, 说明要存到原来的旧叶节点内的第0个位置上, 而且要向上更新结点
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
	//取得NewBlock最小的元素, 作为父节点的新分隔符
	delete Target;
	Target = new char[AttrSize];
	memcpy(Target, NewBlock->Content + 9, AttrSize);
	//开始沿叶节点返回到根节点, 判断是否需要分裂
	bool RootSplitted = false;
	if (TrackLength == 0) {//根节点是叶节点, 而且进行了分裂. 直接跳过下面这个循环, 进行根节点分裂相关操作
		RootSplitted = true;
		SplitFlag = false;
	}
	while (SplitFlag) {
		TrackLength--;
		//IndexBlock的第P个子结点分裂(从0开始编号), 分裂出的新子结点的块编号为NewSon. IndexBlock的占用率为Occupancy
		IndexBlock = bm.GetBlock(IndexFile, Track[TrackLength]);
		unsigned short P = Desce[TrackLength];
		unsigned int NewSon = Sibli[TrackLength + 1];
		Occupancy = Occup[TrackLength];
		if (Occupancy <= FanOut - 1) {//占用率≤FanOut - 1, 直接填入
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
			if (IndexBlock->GetOffset() == 0) {//根节点分裂, 后续需要进一步处理
				SplitFlag = false;
				RootSplitted = true;
			}
			//取得新块
			NewBlock = GetNewBlock(index);
			NewBlock->Content[0] = 0;
			Sibli[TrackLength] = NewBlock->GetOffset();
			//设置占用率
			Occupancy = (FanOut + 1) / 2;
			memcpy(IndexBlock->Content + 1, &Occupancy, 2);
			Occupancy = FanOut + 1 - (FanOut + 1) / 2;
			memcpy(NewBlock->Content + 1, &Occupancy, 2);
			//移动块内容
			if (P <= (FanOut + 1) / 2 - 2) {//说明新增的那个属性仍然放在旧的结点内, 旧的结点的第FanOut-1~(FanOut+1)/2-1个属性移到新的结点内
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
			else if (P == (FanOut + 1) / 2 - 1) {//说明新增的那个属性放在新的结点内, 且作为第0个指针出现. 旧的结点的第FanOut-1~(FanOut+1)/2个属性移到新的结点内
				memcpy(NewBlock->Content + 5 + 4, IndexBlock->Content + 5 + (P + 1) * (AttrSize + 4) - AttrSize, (FanOut - (FanOut + 1) / 2) * (AttrSize + 4));
				memcpy(NewBlock->Content + 5, &NewSon, 4);
			}
			else {//说明新增的那个属性放在新的结点内, 且作为第P-(FanOut+1)/2+1个指针出现. 旧的结点的第FanOut-1~(FanOut+1)/2个属性移到新的结点内
				//先把旧的结点的第P+1到FanOut-1个属性拷贝到新结点的第P+2-(FanOut+1)/2 ~ FanOut-(FanOut+1)/2个属性
				//再把分裂的结点拷贝到新结点的第P+1-(FanOut+1)/2个属性处
				//再把旧的结点的第(FanOut+1)/2到P个属性拷贝到新结点的第0 ~ P-(FanOut+1)/2个属性
				//Target存储旧的结点的第(FanOut+1)/2个指针前面的属性
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
	if (RootSplitted) {//如果根节点进行了分裂
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


//在B+树中删除list所对应的一系列结点
void IndexManager::DeleteTuples(const IndexSchema& index, DeletedTuple* list) {
	DeletedTuple* lst = list;
	while (lst) {
		Delete(index, lst);
		lst = lst->Next;
	}
}

//在B+树中删除单个结点
void IndexManager::Delete(const IndexSchema& index, DeletedTuple* node) {
	const void* Value = node->values[index.AttributeNumber];
	//初始化数据
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	unsigned int AttrSize = index.AttrSize();
	unsigned int FanOut = (BlockSize - 4 - 5) / (4 + AttrSize) + 1;
	//开始查找
	//B+树至少有二叉, 整个Index至多有2^16-1=65535块, 因此树高至多15, 沿根节点到叶节点至多16个块
	unsigned int Track[16] = { 0 };//从根节点一直追踪到叶节点, 沿路的块号
	unsigned short Occup[16] = { 0 };//沿路每个块的占用率
	unsigned short Desce[16] = { 0 };//沿路的每个块分别沿第几个指针往下走
	int TrackLength = 0;
	unsigned short Occupancy;
	unsigned int TargetBlock;
	Block* IndexBlock;
	//循环直至遇到叶节点
	while (1) {
		TargetBlock = Track[TrackLength];
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(Occup + TrackLength, IndexBlock->Content + 1, 2);
		Occupancy = Occup[TrackLength];
		//判断是不是叶节点
		if (IndexBlock->Content[0] == 1) //是叶节点
			break;
		void* Target;
		if (index.Type == SQL_INT)
			Target = new int;
		else if (index.Type == SQL_FLOAT)
			Target = new float;
		else
			Target = new string;
		unsigned short P = -1;
		for (unsigned short i = 0; i <= Occupancy - 2; i++) {//非叶结点占用率为Occupancy, 则分隔值有Occupancy-1个, 分别位于IndexBlock->Content[9+i*(AttrSize+4)], i=0,1,...,Occupancy-2
			if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
				memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			else {
				char Buffer[4096];
				memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
				Buffer[AttrSize] = 0;
				*(string*)Target = Buffer;
			}
			if (Compare(Value, Target, index.Type) == -1) {//找到第一个Target, 严格大于Value
				P = i;
				break;
			}
		}
		delete Target;
		if (P == 65535)//如果该结点内所有值都≤Value, 直接取最后一个指针. 否则, 取第P个指针
			P = Occupancy - 1;
		Desce[TrackLength] = P;
		TrackLength++;
		memcpy(Track + TrackLength, IndexBlock->Content + 5 + P * (AttrSize + 4), 4);
	}
	//遇到叶节点, 开始执行删除操作
	void* Target;
	if (index.Type == SQL_INT)
		Target = new int;
	else if (index.Type == SQL_FLOAT)
		Target = new float;
	else
		Target = new string;
	unsigned short DeletedP = -1;//记录删除的位置, 从0~Occupancy-1
	for (int i = 0; ; i++) {
		if (index.Type == SQL_INT || index.Type == SQL_FLOAT)
			memcpy(Target, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
		else {
			char Buffer[4096];
			memcpy(Buffer, IndexBlock->Content + 9 + i * (AttrSize + 4), AttrSize);
			Buffer[AttrSize] = 0;
			*(string*)Target = Buffer;
		}
		if (Compare(Target, Value, index.Type) == 0) {   //相等, 更新DeletedP的值, 跳出循环
			DeletedP = i;
			break;
		}
	}
	//把DeletedP后面的元素整体前移
	for (int i = DeletedP + 1; i <= Occupancy - 1; i++) 
		memcpy(IndexBlock->Content + 5 + (i - 1) * (AttrSize + 4), IndexBlock->Content + 5 + i * (AttrSize + 4), AttrSize + 4);
	Occupancy = --Occup[TrackLength];
	memcpy(IndexBlock->Content + 1, &Occupancy, 2);//占用率--
	bm.VisitBlock(IndexBlock, 1);
	//如果DeletedP=0, 说明删掉的值在第0个位置, 要向上更新结点
	if (!DeletedP) {
		//存储该叶节点新的最小值
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
	//开始合并结点
	if (!TrackLength)//TrackLength=0, 说明根节点就是叶节点, 不做任何操作
		return;
	int MergeDir = 0;//-1:和左边的兄弟结点合并 1:和右边的兄弟结点合并
	while (1) {
		Occupancy = Occup[TrackLength];
		TargetBlock = Track[TrackLength];
		IndexBlock = bm.GetBlock(IndexFile, TargetBlock);
		unsigned short Leaf = (IndexBlock->Content[0]) ? 1 : 0;
		if (TrackLength == 0 || Occupancy >= (FanOut + 1 - Leaf) / 2)//如果当前是根节点, 或者占用率≥(FanOut-Leaf)/2上取整, 则不需要合并
			break;
		//当前结点是Block[FatherOffset]的第P个子结点
		unsigned int FatherOffset = Track[TrackLength - 1];
		unsigned short P = Desce[TrackLength - 1];
		if (P < Occup[TrackLength - 1] - 1)
			MergeDir = 1;//和右边的兄弟结点合并
		else
			MergeDir = -1;//和左边的兄弟结点合并
		Block* FatherBlock = bm.GetBlock(IndexFile, FatherOffset);
		//SiblOffset是兄弟结点的块偏移数
		unsigned short SiblOffset;
		memcpy(&SiblOffset, FatherBlock->Content + 5 + (P + MergeDir) * (AttrSize + 4), 4);
		//SiblBlock是兄弟结点的块指针
		Block* SiblBlock = bm.GetBlock(IndexFile, SiblOffset);
		//SiblOccupancy是兄弟结点的占用率
		unsigned short SiblOccupancy;
		memcpy(&SiblOccupancy, SiblBlock->Content + 1, 2);
		//为了方便后续代码, 我们统一把IndexBlock放在左侧, SiblBlock放在右侧, P对应IndexBlock的指针编号
		if (MergeDir == -1) {
			Block* tempBlock = IndexBlock; IndexBlock = SiblBlock; SiblBlock = tempBlock;
			P--;
			unsigned short tempOcc = Occupancy; Occupancy = SiblOccupancy; SiblOccupancy = tempOcc;
		}
		//判断是否需要合并
		if (Occupancy + SiblOccupancy > FanOut - Leaf) {//不需要合并, 只需要从兄弟结点里匀一部分内容即可. 最后更新父结点
			unsigned short Occ1 = (Occupancy + SiblOccupancy) / 2, Occ2 = Occupancy + SiblOccupancy - (Occupancy + SiblOccupancy) / 2;
			if (Leaf) {//叶节点的处理方式和非叶结点不同
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
			//更新写标志
			bm.VisitBlock(IndexBlock, 1);
			bm.VisitBlock(SiblBlock, 1);
			bm.VisitBlock(FatherBlock, 1);
			break;
		}
		else {//需要和兄弟结点合并. 最后更新父结点
			unsigned short Occ = Occupancy + SiblOccupancy;
			if (Leaf) {//叶节点的处理方式和非叶结点不同
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
			//将父节点第P+1个指针(连带前面的分隔值)删除, 后面全体前移
			for (int i = P + 1; i <= Occup[TrackLength - 1] - 2; i++)
				memcpy(FatherBlock->Content + 5 + i * (AttrSize + 4) - AttrSize, FatherBlock->Content + 5 + (i + 1) * (AttrSize + 4) - AttrSize, AttrSize + 4);
			//父节点占用率--
			Occup[TrackLength - 1]--;
			memcpy(FatherBlock->Content + 1, Occup + TrackLength - 1, 2);
			//更新写标志
			bm.VisitBlock(IndexBlock, 1);
			bm.VisitBlock(FatherBlock, 1);
		}
		TrackLength--;
	}
	if (TrackLength == 0 && Occupancy == 1) {//根节点占用率只有1, 删除该根节点
		memcpy(&TargetBlock, IndexBlock->Content + 5, 4);
		Block* FreeBlock = bm.GetBlock(IndexFile, TargetBlock);
		memcpy(IndexBlock->Content, FreeBlock->Content, 3);
		memcpy(IndexBlock->Content + 5, FreeBlock->Content + 5, BlockSize - 5);
		DeleteBlock(index, FreeBlock);
	}
}

//从空闲列表中得到空闲块
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

//将空闲块挂入空闲列表
void IndexManager::DeleteBlock(const IndexSchema& index, Block* Obj) {
	string IndexFile = IndexDirectory + index.TableName + "/" + index.IndexName + ".Index";
	Block* IndexHead = bm.GetBlock(IndexFile, 0);
	memcpy(Obj->Content + 3, IndexHead->Content + 3, 2);
	unsigned short temp = Obj->GetOffset();
	memcpy(IndexHead->Content + 3, &temp, 2);
	bm.VisitBlock(Obj, 1);
	bm.VisitBlock(IndexHead, 1);
}