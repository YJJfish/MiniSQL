#include <iostream>
#include <fstream>
#include <ctime>

#include "Global.h"
#include "BufferManager.h"


using namespace std;

//Block���캯��
Block::Block(int Hot) :Occupied(0), Hot(Hot), BirthTime(0), VisitTime(0), Filename(""), Offset(0), Changed(0), Content{ 0 }, Last(NULL), Next(NULL) {}

//Block��������
Block::~Block(void) {}

//��ѯ��Block���ļ��ڵ�ƫ�Ƶ�ַ
int Block::GetOffset(void) {
	return this->Offset;
}

//��ѯ��Block��Ӧ���ļ���
string Block::GetFilename(void) {
	return this->Filename;
}

//д��Ӳ��, ����ֵ��ʾ�ɹ����
bool Block::Write(void) {
	if (this->Occupied == 0)
		return false;
	//����Block��Ϣ
	this->Occupied = 0;
	//���ÿ��ڴ�δ���ı�, ��д��Ӳ��
	if (this->Changed == 0)
		return false;
	//�ļ�д��
	fstream fp;
	//�ȼ���ļ��Ƿ����, �Լ��ļ���С�Ƿ����Ҫ��. ����, ˵���Ѿ���ɾ��, ������д��
	int Size = FileSize(this->Filename);
	if (Size == -1 || Size < this->Offset + 1)
		return false;
	fp.open(this->Filename, ios::out | ios::in | ios::binary);  //������׷��д��
	fp.seekg(this->Offset * BlockSize, ios::beg);   //��Ϊ���ÿ����ʼλ��
	fp.write(this->Content, BlockSize);             //д��
	fp.close();
	return true;
}

//��Ӳ�̶�ȡ, ����ֵ��ʾ�ɹ����
bool Block::Read(string Filename, int Offset) {
	if (this->Occupied)
		return false;
	//����Block��Ϣ
	this->BirthTime = this->VisitTime = clock();
	this->Changed = 0;
	this->Filename = Filename;
	this->Occupied = 1;
	this->Offset = Offset;
	//�ļ�����
	fstream fp;
	fp.open(Filename, ios::in | ios::binary);     //�����ƶ���
	fp.seekg(BlockSize * Offset, ios::beg);       //��Ϊ���ÿ����ʼλ��
	fp.read(this->Content, BlockSize);            //����
	fp.close();
	this->Content[BlockSize] = '\0';
	return true;
}








//BufferManager���캯��
BufferManager::BufferManager(void): NewBlock(NULL), OldBlock(NULL) {
	Block* next = NULL, * last = NULL;
    //����OldBlockNum����������
	last = this->OldBlock = new Block(0);
	for (int i = 1; i < OldBlockNum; i++) {
		next = new Block(0);
		last->Next = next;
		next->Last = last;
		last = next;
	}
	last->Next = this->OldBlock;
	this->OldBlock->Last = last;
	//����BlockNum-OldBlockNum����������
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

//BufferManager��������
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
 * ȡ��ĳһ������ڴ�.
 * ������ڻ��������Ѵ��ڣ���ֱ�ӷ���ָ�룻�������ڣ����ȡ���ٷ���ָ��.
 * ��������������������滻��������������.
 * ���ļ�������, ���ļ���С���ǿ��������, ��ƫ��������ָ���ļ��Ĵ�С, ����NULL
 */
Block* BufferManager::GetBlock(string Filename, int Offset) {
	Block* ptr;
	//����������
	if (BlockNum - OldBlockNum && this->NewBlock->Occupied) {
		ptr = this->NewBlock;
		do {
			if (ptr->Filename == Filename && ptr->Offset == Offset) {//�����������ҵ��ÿ�
				this->VisitBlock(ptr, 0);
				return ptr;
			}
			ptr = ptr->Next;
		} while (ptr != this->NewBlock && ptr->Occupied);
	}
	//����������
	if (this->OldBlock->Occupied) {
		ptr = this->OldBlock;
		do {
			if (ptr->Filename == Filename && ptr->Offset == Offset) {//�����������ҵ��ÿ�
				this->VisitBlock(ptr, 0);
				return ptr;
			}
			ptr = ptr->Next;
		} while (ptr != this->OldBlock && ptr->Occupied);
	}
	//��û�ҵ�, ���Ǵ�Ӳ�̶�ȡ, ����������
	//�ȼ����ļ��Ƿ�Ϸ�
	int MaxOffset = FileSize(Filename);
	if (MaxOffset == -1 || MaxOffset <= Offset)
		return NULL;
	//��λ���������һ�����
	this->OldBlock = this->OldBlock->Last;
	//���ÿ鱻ռ��, ����д��Ӳ��
	this->OldBlock->Write();
	//��Ӳ�̶�ȡָ�����ݷ���ÿ�
	this->OldBlock->Read(Filename, Offset);
	return this->OldBlock;
}

/*
 * �ڸ��ļ�ĩβ׷��һ���飬����ȡ������أ����ؿ��ָ��
 * �����ļ��Ĵ�С���ǿ��������, ����NULL
 * ���Filename��λ�������ڵ��ļ�, ����ڴ������½����ļ�, ��׷�ӿ�
 */
Block* BufferManager::AppendBlock(string Filename) {
	//�ȼ����ļ��Ƿ�Ϸ�
	fstream fp;
	fp.open(Filename, ios::out | ios::in | ios::binary); //������׷��д��
	if (fp.fail()) {//�ļ�������
		Touch(Filename);
		fp.open(Filename, ios::out | ios::in | ios::binary); //������׷��д��
	}
	fp.seekg(0, ios::end);
	int pos = fp.tellg();
	if (pos % BlockSize) {
		fp.close();
		return NULL;
	}
	//д��BlockSize��ռλ��
	char buffer[BlockSize];
	memset(buffer, Placeholder, BlockSize);
	fp.write(buffer, BlockSize);
	fp.close();
	//����
	return this->GetBlock(Filename, pos / BlockSize);
}


/*
 * ���������ڵ�ָ����ǿ������������
 * ������ĩβ�Ŀ������������, ��Ϊ������������ͷ�ڵ�
 * �������������ڻ�û������������������False. ���򷵻�True
 */
bool BufferManager::HotBlock(Block* Obj) {
	if (Obj->Hot || BlockNum - OldBlockNum == 0)
		return false;
	if (OldBlockNum == 1 && BlockNum - OldBlockNum == 1) {//������������������ֻ��һ����
		Block* tmp = this->NewBlock;
		//�ȸ���Hotֵ
		tmp->Hot = 0;
		this->OldBlock->Hot = 1;
		//�������
		this->NewBlock = this->OldBlock;
		this->OldBlock = tmp;
	}
	else if (OldBlockNum == 1) {//������ֻ��һ����, ������>=2����
		Block* tmp = this->NewBlock->Last;
		//�ȸ���Hotֵ
		tmp->Hot = 0;
		this->OldBlock->Hot = 1;
		//�������
		tmp->Next->Last = tmp->Last->Next = this->OldBlock;
		this->OldBlock->Next = tmp->Next;
		this->OldBlock->Last = tmp->Last;
		tmp->Next = tmp->Last = tmp;
		this->NewBlock = this->OldBlock;
		this->OldBlock = tmp;
	}
	else if (BlockNum - OldBlockNum == 1) {//������ֻ��һ����, ������>=2����
		Block* tmp = this->NewBlock;
		//�ȸ���Hotֵ
		tmp->Hot = 0;
		Obj->Hot = 1;
		//�������
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
	else {//��������������������>=2����
		Block* tmp = this->NewBlock->Last;
		Block* tmp2 = tmp->Last;
		//�ȸ���Hotֵ
		tmp->Hot = 0;
		Obj->Hot = 1;
		//�������
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
	//���, ����һ���������. ������������˳����������δ��ռ��, ����Ҫ����Ų��������ĩβ
	if (this->OldBlock->Occupied == 0)
		this->OldBlock = this->OldBlock->Next;
	return true;
}

/*
 * ÿ�η��ʸ�Blockʱ����øú���, �������»��������Ϳ����Ϣ
 * RW=false��ʾ��������VisitTime; RW=true��ʾд������VisitTime��Changed
 */
void BufferManager::VisitBlock(Block* Obj, bool RW) {
	if (!RW) { //��
		Obj->VisitTime = clock();
	}
	else {     //д
		Obj->VisitTime = clock();
		Obj->Changed = true;
	}
	if (Obj->Hot) {//λ��������������
		if (Obj != this->NewBlock) {
			//��˫��������ɾ���ý��
			Obj->Last->Next = Obj->Next;
			Obj->Next->Last = Obj->Last;
			//�嵽ͷ��
			Obj->Next = this->NewBlock;
			Obj->Last = this->NewBlock->Last;
			this->NewBlock->Last->Next = Obj;
			this->NewBlock->Last = Obj;
			this->NewBlock = Obj;
		}
	}
	else {        //λ��������������
		if (Obj->VisitTime - Obj->BirthTime > CLOCKS_PER_SEC * OldBlockTime / 1000 && BlockNum - OldBlockNum)//�����������ڴ���ʱ������ָ��ֵ
			this->HotBlock(Obj);//����������
		else if (Obj != this->OldBlock) {
			//��˫��������ɾ���ý��
			Obj->Last->Next = Obj->Next;
			Obj->Next->Last = Obj->Last;
			//�嵽ͷ��
			Obj->Next = this->OldBlock;
			Obj->Last = this->OldBlock->Last;
			this->OldBlock->Last->Next = Obj;
			this->OldBlock->Last = Obj;
			this->OldBlock = Obj;
		}
	}
}

/*
 * Debug�ã���������������Ϣ
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


