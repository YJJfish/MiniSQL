#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <io.h>

using namespace std;

//***********************************************************ȫ�ֳ�������***********************************************************

//������һ����Ĵ�С, ��λΪByte
#define BlockSize 4096

//��������ĸ���
#define BlockNum 100

//��������ĸ�������OldBlockNum����BlockNumʱ���㷨�˻�����ͨ��LRU
#define OldBlockNum 37

//һ���������������ؽ������������������ͣ����ʱ��, ��λΪ����
#define OldBlockTime 1000

//�����ֽ�ռλ��
#define Placeholder '\0'

//Ԫ�������ļ�Ŀ¼
#define DataDirectory "./Table/"

//ģʽ�����ļ�Ŀ¼
#define SchemasDirectory "./Catalog/"

//���������ļ�Ŀ¼
#define IndexDirectory "./Index/"

//��ѯ����ļ�Ŀ¼
#define ResultDirectory "./Result/"

//��ѯ����op�ı���
#define eq 0
#define neq 1
#define les 2
#define greater 3
#define leq 4
#define geq 5

//***********************************************************ȫ�����Ͷ���***********************************************************

#define SQL_INT 0
#define SQL_FLOAT 1
#define SQL_CHAR 2

//���ģʽ��Ϣ����Catalog Manager��API��Record Manager��Index Managerʹ��
//�ļ��Ķ�ȡ��д����Catalog Manager���𣬲���API���ա�����Record Manager��Index Manager
//����ÿ�������һ��������������UniqueԼ��Ҳֻ�ܽ����ڵ�������
class TableSchema {
public:
    string TableName;				//��������������Ϊ256�ֽ�(����ĩβ'\0')
    unsigned char NumAttributes;	//���Ը���, ռ1�ֽ�. 1<=���Ը���<=255 (ʵ��ָ����Ҫ��32��)
    string* AttributeName;			//��ĸ�����������ʹ�ö�̬�ڴ��������ռ䡣ÿ��string��������Ϊ256�ֽ�(����ĩβ'\0')
    bool* Unique;					//��������Ƿ�Unique��ռNumAttributes���ֽ�
    unsigned char* Type;			//ÿ�����Ե����, ֧��SQL_INT��SQL_FLOAT��SQL_CHAR
    unsigned char* Length;			//�����char���, ����Ҫ��¼�䳤�ȣ��������ͣ���0���ɡ�������ƿ������պ���չ, ����Type=SQL_INTʱ��Length���������ķ�Χ(short int, long int��)
    unsigned char Primary;			//��ʶ�ڼ�������������

    TableSchema(void) : TableName(""), NumAttributes(0), AttributeName(NULL), Unique(NULL), Type(NULL), Length(NULL), Primary(0) {}
    ~TableSchema(void) {
        if (AttributeName)
            delete[] AttributeName;
        if (Unique)
            delete[] Unique;
        if (Type)
            delete[] Type;
        if (Length)
            delete[] Length;
    }
    //����ڴ�
    void Free(void) {
        if (AttributeName)
            delete[] AttributeName;
        if (Unique)
            delete[] Unique;
        if (Type)
            delete[] Type;
        if (Length)
            delete[] Length;
        TableName = "";
        NumAttributes = 0;
        Primary = 0;
        AttributeName = NULL;
        Unique = NULL;
        Type = NULL;
        Length = NULL;
    }
    //�Ӹ������ļ����ж�ȡ. fin������fin.open(<�ļ���>, ios::in | ios::binary)�ķ�ʽ��
    void Read(fstream& fin) {
        Free();
        unsigned char len = 0;
        char buffer[257] = { 0 };
        //��TableName
        fin.read((char*)&len, 1);
        fin.read(buffer, len); buffer[len] = 0;
        this->TableName = string(buffer);
        //��NumAttributes
        fin.read((char*)&(this->NumAttributes), 1);
        //��AttributeName
        this->AttributeName = new string[this->NumAttributes];
        for (int i = 0; i < this->NumAttributes; i++) {
            fin.read((char*)&len, 1);
            fin.read(buffer, len); buffer[len] = 0;
            this->AttributeName[i] = string(buffer);
        }
        //��Unique
        this->Unique = new bool[this->NumAttributes];
        fin.read((char*)this->Unique, this->NumAttributes);
        //��Type
        this->Type = new unsigned char[this->NumAttributes];
        fin.read((char*)this->Type, this->NumAttributes);
        //��Length
        this->Length = new unsigned char[this->NumAttributes];
        fin.read((char*)this->Length, this->NumAttributes);
        //��Primary
        fin.read((char*)&(this->Primary), 1);
    }
    //д��ָ�����ļ���. fout������fin.open(<�ļ���>, ios::out | ios::in | ios::binary)�ķ�ʽ��
    void Write(fstream& fout) const{
        unsigned char len = 0;
        //дTableName
        len = this->TableName.length();
        fout.write((char*)&len, 1);
        fout.write(this->TableName.c_str(), len);
        //дNumAttributes
        fout.write((char*)&(this->NumAttributes), 1);
        //дAttributeName
        for (int i = 0; i < this->NumAttributes; i++) {
            len = this->AttributeName[i].length();
            fout.write((char*)&len, 1);
            fout.write(this->AttributeName[i].c_str(), len);
        }
        //дUnique
        fout.write((char*)this->Unique, this->NumAttributes);
        //дType
        fout.write((char*)this->Type, this->NumAttributes);
        //дLength
        fout.write((char*)this->Length, this->NumAttributes);
        //дPrimary
        fout.write((char*)&(this->Primary), 1);
    }

    //����table���AttrNum��������ռ�õ��ֽڴ�С
    int AttrSize(int AttrNum) const {
        return (Type[AttrNum] == SQL_INT || Type[AttrNum] == SQL_FLOAT) ? 4 : Length[AttrNum];
    }

    //����table����Ԫ����ռ�õ��ֽڴ�С
    int RecordSize(void) const {
        int sum = 0;
        for (int i = 0; i < NumAttributes; i++) {
            sum += AttrSize(i);
        }
        return (sum < 4) ? 5 : sum + 1;
    }

    //����table���AttrNum������������Ԫ���е��ֽ�ƫ��
    int AttrOffset(int AttrNum) const {
        int sum = 0;
        for (int i = 0; i < AttrNum; i++) {
            sum += AttrSize(i);
        }
        return sum + 1;
    }

    friend class API;
    friend class Interpreter;
    friend class CatelogManager;
    friend class RecordManager;
    friend class IndexManager;
};




//������ģʽ��Ϣ����Catalog Manager��API��Index Managerʹ��
//�ļ��Ķ�ȡ��д����Catalog Manager���𣬲���API���ա�����Index Manager
//��������ֻ�ܽ����ھ���UniqueԼ���ĵ�������
class IndexSchema {
public:
    string IndexName;				//����������������Ϊ256�ֽ�(����ĩβ'\0')
    string TableName;				//��������������Ϊ256�ֽ�(����ĩβ'\0')
    string AttributeName;			//����������������Ϊ256�ֽ�(����ĩβ'\0')
    unsigned char AttributeNumber;	//���Ա��, ��ʾ���������ԭ���еĵڼ�������. ��0����. ������������, ���ǿ��Լӿ��������
    unsigned char Type;				//���Ե����. ������������, ���ǿ��Լӿ��������
    unsigned char Length;			//���Եĳ���(����char����). ������������, ���ǿ��Լӿ��������


    IndexSchema(void) :IndexName(""), TableName(""), AttributeName(""), AttributeNumber(0), Type(-1), Length(0) {}
    ~IndexSchema(void) {}
    //�Ӹ������ļ����ж�ȡ. fin������fin.open(<�ļ���>, ios::in | ios::binary)�ķ�ʽ��
    void Read(fstream& fin) {
        unsigned char len = 0;
        char buffer[257] = { 0 };
        //��IndexName
        fin.read((char*)&len, 1);
        fin.read(buffer, len); buffer[len] = 0;
        this->IndexName = string(buffer);
        //��TableName
        fin.read((char*)&len, 1);
        fin.read(buffer, len); buffer[len] = 0;
        this->TableName = string(buffer);
        //��AttributeName
        fin.read((char*)&len, 1);
        fin.read(buffer, len); buffer[len] = 0;
        this->AttributeName = string(buffer);
        //��AttributeNumber
        fin.read((char*)&(this->AttributeNumber), 1);
        //��Type
        fin.read((char*)&(this->Type), 1);
        //��Length
        fin.read((char*)&(this->Length), 1);
    }
    //д��ָ�����ļ���. fout������fin.open(<�ļ���>, ios::out | ios::in | ios::binary)�ķ�ʽ��
    void Write(fstream& fout) const{
        unsigned char len = 0;
        //дIndexName
        len = this->IndexName.length();
        fout.write((char*)&len, 1);
        fout.write(this->IndexName.c_str(), len);
        //дTableName
        len = this->TableName.length();
        fout.write((char*)&len, 1);
        fout.write(this->TableName.c_str(), len);
        //дAttributeName
        len = this->AttributeName.length();
        fout.write((char*)&len, 1);
        fout.write(this->AttributeName.c_str(), len);
        //дAttributeNumber
        fout.write((char*)&(this->AttributeNumber), 1);
        //дType
        fout.write((char*)&(this->Type), 1);
        //дLength
        fout.write((char*)&(this->Length), 1);
    }

    //���ظ�������������ռ�õ��ֽڴ�С
    int AttrSize(void) const {
        return (Type == SQL_INT || Type == SQL_FLOAT) ? 4 : Length;
    }

    friend class API;
    friend class Interpreter;
    friend class CatelogManager;
    friend class RecordManager;
    friend class IndexManager;
};

//select�����ʱ������
//��ʡ�ڴ�, ���ٲ���Ҫ�Ĵ���IO, ����ֻ��¼��ƫ����/�ֽ�ƫ����
//ʹ������洢, ��Ϊ��ѯǰ����֪����ѯ�����������Ԫ��

class Pos {
public:
    unsigned short BlockOffset;
    unsigned short ByteOffset;
    Pos* Next;
    Pos(void) :BlockOffset(-1), ByteOffset(-1), Next(0) {}
    ~Pos(void) {}
};

//delete�����ʱ������
//��class Pos��һ������������¼Ԫ��ľ�����Ϣ����Ϊ���������RecordManagerɾ����ЩԪ��󷵻ص�
//ʹ������洢, ��Ϊ��ѯǰ����֪����ѯ�����������Ԫ��
class DeletedTuple {
public:
    void** values;
    DeletedTuple* Next;
    DeletedTuple(void) : values(0), Next(0) {}
    ~DeletedTuple(void) {}
};


//***********************************************************���ú�������***********************************************************

/*
 * ���ɿ��ļ�/�ļ���.
 * Filename����'/'��β��������ļ���, ������������ļ�.
 * ��·��������Ҳ��ǿ������.
 * ����./Test/Test.txt, ���û��./Test����ļ���, �������ɸ��ļ���, Ȼ�����ļ���������Test.txt
 */
inline void Touch(string Filename) {
    int Last;
    for (Last = Filename.length() - 1; Last >= 0 && Filename[Last] != '/'; Last--);
    if (Last == -1) {//�����ַ����ڶ�û��'/', ֱ�Ӵ������ļ�
        fstream fp;
        fp.open(Filename, ios::out);
        fp.close();
    }
    else {//�ȴ����ļ���
        system((string("md \"") + Filename.substr(0, Last) + "\"").c_str());
        if (Last < Filename.length() - 1) {//���Buffer������'/'��β, ˵����Ҫ�������ļ�
            fstream fp;
            fp.open(Filename, ios::out);
            fp.close();
        }
    }
}


/*
 * ɾ��ָ���ļ����ļ���.
 * ���Filename��'/'��β, �����ɾ���ļ���, �Լ��ļ�������������.
 * ����, ��ɾ����Ӧ�ļ�.
 */
inline void Remove(string Filename) {
    int Last;
    char Buffer[1024];
    for (int i = 0; i < Filename.length(); i++)
        if (Filename[i] == '/') Buffer[i] = '\\';
        else Buffer[i] = Filename[i];
    Buffer[Filename.length()] = '\0';
    for (Last = Filename.length() - 1; Last >= 0 && Filename[Last] != '/'; Last--);
    if (Last == -1 || Last != Filename.length() - 1)//�����ַ����ڶ�û��'/', ���ַ�������'/'��β, ֱ��ɾ���ļ�
        system(("del \"" + string(Buffer) + "\"").c_str());
    else //ɾ���ļ����Լ��ļ�������������
        system(("rd /s/q \"" + Filename + "\"").c_str());
}

/*
 * �����ļ�������, ���ļ���С���ǿ��������, �򷵻�-1
 * ���򣬷��ظ��ļ��ڴ�����ռ�õ��ܿ���
 * ʵ���Ͼ����ļ����ݴ�С/���С
 */
inline int FileSize(string Filename) {
    fstream fp;
    fp.open(Filename, ios::in | ios::binary);
    if (fp.fail())
        return -1;
    fp.seekg(0, ios::end);
    if (fp.tellg() % BlockSize) {
        fp.close();
        return -1;
    }
    int ans = fp.tellg() / BlockSize;
    fp.close();
    return ans;
}

/*
 * ɾ��Ŀ¼Path����������ΪFilename���ļ�
 */
static void DeleteFiles(string Path, string Filename) {
    //�ļ����
    intptr_t  hFile = 0;
    //�ļ���Ϣ
    struct _finddata_t fileinfo;
    string p;
    if ((hFile = _findfirst(p.assign(Path).append("/*").c_str(), &fileinfo)) != -1) {
        do {
            //�����Ŀ¼, �ݹ�
            //�������, ���ж��Ƿ���Ŀ���ļ�, Ȼ��ɾ��
            if ((fileinfo.attrib & _A_SUBDIR)) {
                if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
                    DeleteFiles(p.assign(Path).append("/").append(fileinfo.name), Filename);
            }
            else if (fileinfo.name == Filename) {
                Remove(p.assign(Path).append("/").append(fileinfo.name));
            }
        } while (_findnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
}

/*
 * �ж�Ŀ¼Path���Ƿ�������ΪFilename���ļ�
 */
static bool FilesExist(string Path, string Filename) {
    //�ļ����
    intptr_t  hFile = 0;
    //�ļ���Ϣ
    struct _finddata_t fileinfo;
    string p;
    if ((hFile = _findfirst(p.assign(Path).append("/*").c_str(), &fileinfo)) != -1) {
        do {
            //�����Ŀ¼, �ݹ�
            //�������, ���ж��Ƿ���Ŀ���ļ�, Ȼ��ɾ��
            if ((fileinfo.attrib & _A_SUBDIR)) {
                if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0 && FilesExist(p.assign(Path).append("/").append(fileinfo.name), Filename))
                    return true;
            }
            else if (fileinfo.name == Filename) {
                return true; //��ΪFilename���ļ�����
            }
        } while (_findnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
    return false;
}

/*
 * �Ƚ�����ֵ�Ĵ�С, ֵ��������Typeȷ��.
 * SQL_INT: ���void*���ͳ�int*;
 * SQL_FLOAT: ���void*���ͳ�float*;
 * SQL_CHAR: ���void*���ͳ�string*.
 * ����ֵ: ����: 1; ����: 0; С��: -1.
 */
inline int Compare(const void* p1, const void* p2, unsigned char Type) {
    if (p1 == NULL || p2 == NULL)
        return -1;
    if (Type == SQL_INT) {
        if (*((int*)p1) < *((int*)p2))
            return -1;
        else if (*((int*)p1) == *((int*)p2))
            return 0;
        else
            return 1;
    }
    else if (Type == SQL_FLOAT) {
        if (*((float*)p1) < *((float*)p2))
            return -1;
        else if (*((float*)p1) == *((float*)p2))
            return 0;
        else
            return 1;
    }
    else {
        if (*((string*)p1) < *((string*)p2))
            return -1;
        else if (*((string*)p1) == *((string*)p2))
            return 0;
        else
            return 1;
    }
}

inline bool ValidFloat(string s) {
    int start = 0, l = s.size(), i;
    for (i = 0; s[i] == '+' || s[i] == '-'; i++);
    start = i;
    for (i = start; s[i] && s[i] >= '0' && s[i] <= '9'; i++);//�ҵ�������λ����, ��һ���������ַ�
    if (!s[i]) return true;//��һ������С���������, ����true
    if (s[i] != '.') return false;//���������ַ�Ҳ����С����, ����
    start = i + 1;
    for (i = start; s[i]; i++)
        if (s[i] < '0' || s[i] > '9')
            return false;//С�������ֻ��������, ���򱨴�
    return true;
}

inline bool ValidInt(string s) {
    int i, start;
    for (i = 0; s[i] == '+' || s[i] == '-'; i++);
    start = i;
    for (; s[i]; i++)
        if (s[i] < '0' || s[i]>'9')
            return false;
    //ע��, ��Ҫ�ж�һ���������, ��sֻ����'+' ��'-'
    if (i == start) return false;
    return true;
}