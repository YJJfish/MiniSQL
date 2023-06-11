#include <iostream>
#include <fstream>
#include <ctime>

#include "Global.h"
#include "BufferManager.h"


using namespace std;

//Block构造函数
Block::Block(int Hot) :Occupied(0), Hot(Hot), BirthTime(0), VisitTime(0), Filename(""), Offset(0), Changed(0), Content{ 0 }, Last(NULL), Next(NULL) {}

//Block析构函数
Block::~Block(void) {}

//查询该Block在文件内的偏移地址
int Block::GetOffset(void) {
	return this->Offset;
}

//查询该Block对应的文件名
string Block::GetFilename(void) {
	return this->Filename;
}

//写入硬盘, 返回值表示成功与否
bool Block::Write(void) {
	if (this->Occupied == 0)
		return false;
	//更新Block信息
	this->Occupied = 0;
	//若该块内存未被改变, 不写到硬盘
	if (this->Changed == 0)
		return false;
	//文件写出
	fstream fp;
	//先检查文件是否存在, 以及文件大小是否符合要求. 否则, 说明已经被删除, 不能再写回
	int Size = FileSize(this->Filename);
	if (Size == -1 || Size < this->Offset + 1)
		return false;
	fp.open(this->Filename, ios::out | ios::in | ios::binary);  //二进制追加写出
	fp.seekg(this->Offset * BlockSize, ios::beg);   //定为到该块的起始位置
	fp.write(this->Content, BlockSize);             //写出
	fp.close();
	return true;
}

//从硬盘读取, 返回值表示成功与否
bool Block::Read(string Filename, int Offset) {
	if (this->Occupied)
		return false;
	//更新Block信息
	this->BirthTime = this->VisitTime = clock();
	this->Changed = 0;
	this->Filename = Filename;
	this->Occupied = 1;
	this->Offset = Offset;
	//文件读入
	fstream fp;
	fp.open(Filename, ios::in | ios::binary);     //二进制读入
	fp.seekg(BlockSize * Offset, ios::beg);       //定为到该块的起始位置
	fp.read(this->Content, BlockSize);            //读入
	fp.close();
	this->Content[BlockSize] = '\0';
	return true;
}








//BufferManager构造函数
BufferManager::BufferManager(void): NewBlock(NULL), OldBlock(NULL) {
	Block* next = NULL, * last = NULL;
    //生成OldBlockNum个老生代块
	last = this->OldBlock = new Block(0);
	for (int i = 1; i < OldBlockNum; i++) {
		next = new Block(0);
		last->Next = next;
		next->Last = last;
		last = next;
	}
	last->Next = this->OldBlock;
	this->OldBlock->Last = last;
	//生成BlockNum-OldBlockNum个新生代块
	if (BlockNum - OldBlockNum == 0)
		return;
	last = this->NewBlock = new Block(1);
	for (int i = 1; i < BlockNum - OldBlockNum; i++) {
		next = new Block(1);
		last->Next = next;
		next->Last = last;
		last = next;
	}
	last->Next = this->NewBlock;
	this->NewBlock->Last = last;
}

//BufferManager析构函数
BufferManager::~BufferManager(void) {
	Block* ptr;
	for (int i = 0; i < BlockNum - OldBlockNum; i++) {
		ptr = this->NewBlock;
		this->NewBlock = ptr->Next;
		ptr->Write();
		delete ptr;
	}
	for (int i = 0; i < OldBlockNum; i++) {
		ptr = this->OldBlock;
		this->OldBlock = ptr->Next;
		ptr->Write();
		delete ptr;
	}
}

/*
 * 取得某一个块的内存.
 * 如果块在缓冲区内已存在，则直接返回指针；若不存在，则读取后再返回指针.
 * 若缓冲区已满，会根据替换规则抛弃其他块.
 * 若文件不存在, 或文件大小不是块的整数倍, 或偏移量超出指定文件的大小, 返回NULL
 */
Block* BufferManager::GetBlock(string Filename, int Offset) {
	Block* ptr;
	//搜索新生代
	if (BlockNum - OldBlockNum && this->NewBlock->Occupied) {
		ptr = this->NewBlock;
		do {
			if (ptr->Filename == Filename && ptr->Offset == Offset) {//在新生代内找到该块
				this->VisitBlock(ptr, 0);
				return ptr;
			}
			ptr = ptr->Next;
		} while (ptr != this->NewBlock && ptr->Occupied);
	}
	//搜索老生代
	if (this->OldBlock->Occupied) {
		ptr = this->OldBlock;
		do {
			if (ptr->Filename == Filename && ptr->Offset == Offset) {//在老生代内找到该块
				this->VisitBlock(ptr, 0);
				return ptr;
			}
			ptr = ptr->Next;
		} while (ptr != this->OldBlock && ptr->Occupied);
	}
	//都没找到, 于是从硬盘读取, 放入老生代
	//先检查该文件是否合法
	int MaxOffset = FileSize(Filename);
	if (MaxOffset == -1 || MaxOffset <= Offset)
		return NULL;
	//定位到链表最后一个结点
	this->OldBlock = this->OldBlock->Last;
	//若该块被占用, 将其写回硬盘
	this->OldBlock->Write();
	//从硬盘读取指定内容放入该块
	this->OldBlock->Read(Filename, Offset);
	return this->OldBlock;
}

/*
 * 在该文件末尾追加一个块，并读取到缓冲池，返回块的指针
 * 若该文件的大小不是块的整数倍, 返回NULL
 * 如果Filename定位到不存在的文件, 则会在磁盘内新建该文件, 再追加块
 */
Block* BufferManager::AppendBlock(string Filename) {
	//先检查该文件是否合法
	fstream fp;
	fp.open(Filename, ios::out | ios::in | ios::binary); //二进制追加写出
	if (fp.fail()) {//文件不存在
		Touch(Filename);
		fp.open(Filename, ios::out | ios::in | ios::binary); //二进制追加写出
	}
	fp.seekg(0, ios::end);
	int pos = fp.tellg();
	if (pos % BlockSize) {
		fp.close();
		return NULL;
	}
	//写出BlockSize个占位符
	char buffer[BlockSize];
	memset(buffer, Placeholder, BlockSize);
	fp.write(buffer, BlockSize);
	fp.close();
	//读入
	return this->GetBlock(Filename, pos / BlockSize);
}


/*
 * 将老生代内的指定块强制移入新生代
 * 新生代末尾的块则进入老生代, 成为老生代的链表头节点
 * 若已在新生代内或没有设置新生代，返回False. 否则返回True
 */
bool BufferManager::HotBlock(Block* Obj) {
	if (Obj->Hot || BlockNum - OldBlockNum == 0)
		return false;
	if (OldBlockNum == 1 && BlockNum - OldBlockNum == 1) {//新生代和老生代都各只有一个块
		Block* tmp = this->NewBlock;
		//先更新Hot值
		tmp->Hot = 0;
		this->OldBlock->Hot = 1;
		//链表操作
		this->NewBlock = this->OldBlock;
		this->OldBlock = tmp;
	}
	else if (OldBlockNum == 1) {//老生代只有一个块, 新生代>=2个块
		Block* tmp = this->NewBlock->Last;
		//先更新Hot值
		tmp->Hot = 0;
		this->OldBlock->Hot = 1;
		//链表操作
		tmp->Next->Last = tmp->Last->Next = this->OldBlock;
		this->OldBlock->Next = tmp->Next;
		this->OldBlock->Last = tmp->Last;
		tmp->Next = tmp->Last = tmp;
		this->NewBlock = this->OldBlock;
		this->OldBlock = tmp;
	}
	else if (BlockNum - OldBlockNum == 1) {//新生代只有一个块, 老生代>=2个块
		Block* tmp = this->NewBlock;
		//先更新Hot值
		tmp->Hot = 0;
		Obj->Hot = 1;
		//链表操作
		tmp->Next = this->OldBlock;
		tmp->Last = this->OldBlock->Last;
		this->OldBlock->Last = this->OldBlock->Last->Next = tmp;
		Obj->Next->Last = Obj->Last;
		Obj->Last->Next = Obj->Next;
		Obj->Next = Obj;
		Obj->Last = Obj;
		this->NewBlock = Obj;
		this->OldBlock = tmp;
	}
	else {//新生代和老生代都各自>=2个块
		Block* tmp = this->NewBlock->Last;
		Block* tmp2 = tmp->Last;
		//先更新Hot值
		tmp->Hot = 0;
		Obj->Hot = 1;
		//链表操作
		tmp->Next = this->OldBlock;
		tmp->Last = this->OldBlock->Last;
		this->OldBlock->Last->Next = tmp;
		this->OldBlock->Last = tmp;
		tmp2->Next = Obj;
		this->NewBlock->Last = Obj;
		Obj->Last->Next = Obj->Next;
		Obj->Next->Last = Obj->Last;
		Obj->Next = this->NewBlock;
		Obj->Last = tmp2;
		this->NewBlock = Obj;
		this->OldBlock = tmp;
	}
	//最后, 考虑一种特殊情况. 如果从新生代退出来的这个块未被占用, 则需要把它挪到老生代末尾
	if (this->OldBlock->Occupied == 0)
		this->OldBlock = this->OldBlock->Next;
	return true;
}

/*
 * 每次访问该Block时请调用该函数, 用来更新缓冲池链表和块的信息
 * RW=false表示读，更新VisitTime; RW=true表示写，更新VisitTime和Changed
 */
void BufferManager::VisitBlock(Block* Obj, bool RW) {
	if (!RW) { //读
		Obj->VisitTime = clock();
	}
	else {     //写
		Obj->VisitTime = clock();
		Obj->Changed = true;
	}
	if (Obj->Hot) {//位于新生代链表内
		if (Obj != this->NewBlock) {
			//从双向链表中删除该结点
			Obj->Last->Next = Obj->Next;
			Obj->Next->Last = Obj->Last;
			//插到头部
			Obj->Next = this->NewBlock;
			Obj->Last = this->NewBlock->Last;
			this->NewBlock->Last->Next = Obj;
			this->NewBlock->Last = Obj;
			this->NewBlock = Obj;
		}
	}
	else {        //位于老生代链表内
		if (Obj->VisitTime - Obj->BirthTime > CLOCKS_PER_SEC * OldBlockTime / 1000 && BlockNum - OldBlockNum)//若在老生代内存续时长超过指定值
			this->HotBlock(Obj);//放入新生代
		else if (Obj != this->OldBlock) {
			//从双向链表中删除该结点
			Obj->Last->Next = Obj->Next;
			Obj->Next->Last = Obj->Last;
			//插到头部
			Obj->Next = this->OldBlock;
			Obj->Last = this->OldBlock->Last;
			this->OldBlock->Last->Next = Obj;
			this->OldBlock->Last = Obj;
			this->OldBlock = Obj;
		}
	}
}

/*
 * Debug用，输出两条链表的信息
 */
void BufferManager::Debug(void) {
	cout << "-------------------------<Buffer Manager Debug Info>-------------------------" << endl;
	cout << "BlockSize: " << BlockSize << "Bytes  NewBlockNum: " << BlockNum - OldBlockNum << "  OldBlockNum: " << OldBlockNum << "  OldBlockRemainTime: " << OldBlockTime << "ms" << endl;
	if (BlockNum - OldBlockNum) {
		cout << "NewBlockList: " << endl << "+++++++++++++++++" << endl;
		Block* tmp = this->NewBlock;
		do {
			if (tmp->Occupied == 0) cout << "Vacant" << endl;
			else {
				cout << "Hot: " << tmp->Hot << endl;
				cout << "Filename: " << tmp->Filename << endl;
				cout << "Offset: " << tmp->Offset << endl;
				cout << "BirthTime: " << tmp->BirthTime << "  VisitTime: " << tmp->VisitTime << endl;
				cout << "Content: " << tmp->Content << endl;
			}
			tmp = tmp->Next;
			cout << "+++++++++++++++++" << endl;
		} while (tmp != this->NewBlock);
	}
	cout << "OldBlockList: " << endl << "+++++++++++++++++" << endl;
	Block* tmp = this->OldBlock;
	do {
		if (tmp->Next->Last != tmp) cout << "Error!!!!!!!!!!!!!!!!!!!\n";
		if (tmp->Occupied == 0) cout << "Vacant" << endl;
		else {
			cout << "Hot: " << tmp->Hot << endl;
			cout << "Filename: " << tmp->Filename << endl;
			cout << "Offset: " << tmp->Offset << endl;
			cout << "BirthTime: " << tmp->BirthTime << "  VisitTime: " << tmp->VisitTime << endl;
			cout << "Content: " << tmp->Content << endl;
		}
		tmp = tmp->Next;
		cout << "+++++++++++++++++" << endl;
	} while (tmp != this->OldBlock);
}


