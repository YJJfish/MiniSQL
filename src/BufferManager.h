#pragma once
#include "Global.h"
#include <iostream>
using namespace std;

//块
class Block {
public:
	char Content[BlockSize + 1];//块的内容, 固定为BlockSize个字节，其中第BlockSize+1个字节即Content[BlockSize]等于'\0'
	int GetOffset(void);        //查询该Block在文件内的偏移地址
	string GetFilename(void);   //查询该Block对应的文件名
	
private:
	bool Occupied;           //当前是否被占用
	bool Hot;                //是否位于新生代缓冲区
	int BirthTime;           //载入内存的时间
	int VisitTime;           //最近访问时间
	string Filename;         //文件名
	int Offset;              //块在文件内的偏移量, 从0开始计数. 例如等于0时表示从文件开头读入. 必须<=该文件总块数-1.
	bool Changed;            //是否被修改。未被修改过的块被抛弃时不会重新写回磁盘
	Block* Last, * Next;     //链表
	Block(int Hot);          //构造函数
	~Block(void);            //析构函数
	bool Write(void);                         //写入硬盘, 返回值表示成功与否
	bool Read(string Filename, int Offset);   //从硬盘读取, 返回值表示成功与否

	friend class BufferManager;
};



//缓冲区
class BufferManager {
public:
	BufferManager(void);     //构造函数
	~BufferManager(void);    //析构函数

	/*
	 * 取得某一个块的内存. 
	 * 如果块在缓冲区内已存在，则直接返回指针；若不存在，则读取后再返回指针.
	 * 若缓冲区已满，会根据替换规则抛弃其他块.
	 * 若文件不存在, 或文件大小不是块的整数倍, 或偏移量超出指定文件的大小, 返回NULL
	 */
	Block* GetBlock(string Filename, int Offset);

	/*
	 * 在该文件末尾追加一个块，并读取到缓冲池，返回块的指针
	 * 若该文件的大小不是块的整数倍, 返回NULL
	 * 如果Filename定位到不存在的文件, 则会在磁盘内新建该文件, 再追加块
	 */
	Block* AppendBlock(string Filename);

	/*
	 * 将老生代内的指定块强制移入新生代
	 * 新生代末尾的块则进入老生代, 成为老生代的链表头节点
	 * 若已在新生代内或没有设置新生代，返回False. 否则返回True
	 */
	bool HotBlock(Block* Obj);

	/*
	 * 每次访问该Block时请调用该函数, 用来更新缓冲池链表和块的信息
	 * RW=false表示读，更新VisitTime; RW=true表示写，更新VisitTime和Changed
	 */
	void VisitBlock(Block* Obj, bool RW);

	/*
	 * Debug用，输出两条链表的信息
	 */
	void Debug(void);

private:
	Block* NewBlock; //新生代链表头指针
	Block* OldBlock; //老生代链表头指针
};
