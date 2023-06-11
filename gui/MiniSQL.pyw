import tkinter as tk #GUI
from tkinter import ttk as ttk#GUI
import tkinter.filedialog as tkfile #文件资源访问
import tkinter.messagebox as tkmsg #对话框
import ctypes #C语言接口
import re #字符串预处理
import os #用于文件操作
#定义颜色
RootColor="#FFFFFF"
FrameColor="#F5F5F5"
#导入动态链接库
dll = ctypes.cdll.LoadLibrary("./MiniSQL.dll")
#定义Exec函数的传入参数类型
dll.Exec.argtype=ctypes.c_char_p
#定义GetState的返回参数类型
dll.GetState.restype=ctypes.c_int
#定义GetMessage的返回参数类型
dll.GetMessage.restype=ctypes.c_char_p
#定义GetTime的返回参数类型
dll.GetTime.restype=ctypes.c_int
#定义GetCounter的返回参数类型
dll.GetCounter.restype=ctypes.c_int
#定义Quit的返回参数类型
dll.Quit.restype=ctypes.c_int
#DBMS初始化
#dll.Reset()
dll.Init()
def Exit():
    global Codebox
    Quit=dll.Quit()
    if(Quit == 0):
        dll.Exec(b"quit;")
    root.destroy()




#代码编辑栏
#顶部：已打开的文件的标签
#中部：编辑栏
#底部：横向滚动轴
#右侧：水平滚动轴
class CodeFrame:
    def __init__(self, Height=400, Width=1000, ButtonNum=15, ButtonRelHeight=0.06, ScrollbarRelWidth=0.02):
        self.Height=Height #代码编辑栏高度
        self.Width=Width   #代码编辑栏宽度
        self.ButtonNum=ButtonNum #最多同时打开的文件个数
        self.ButtonRelHeight=ButtonRelHeight #文件标签相对高度
        self.ScrollbarRelWidth=ScrollbarRelWidth #滚动轴相对宽度
        self.Frame=tk.Frame(root, background=FrameColor)
        self.Text=[] #(Text, BarX, BarY, Button, Filepath)
        self.Len=0
        self.Highlighted=-1
        self.Current=tk.IntVar()
        self.Current.set(-1)
    def Openfile(self, filepath=None): #打开已有文件
        if (self.Len >= self.ButtonNum):
            tkmsg.showerror(title="Error!", message="You can at most open "+str(self.ButtonNum)+
                            " files.\nPlease close some files in order to open a new file.")
            return
        if (filepath is None):
            filepath=tkfile.askopenfilename(title=u'选择SQL文件',filetypes=[('SQL文件','.sql')])
            if (filepath == ""):
                return
        for i in range(len(self.Text)):
            if (self.Text[i][4] == filepath):
                self.Current.set(i)
                self.Unhighlight()
                self.Highlight(i)
                return
        finalappear=filepath.rfind("/")
        filename=filepath[finalappear+1:-4] if (finalappear!=-1) else filepath[:-4]
        BarX=tk.Scrollbar(self.Frame, orient=tk.HORIZONTAL)
        BarY=tk.Scrollbar(self.Frame, orient=tk.VERTICAL)
        Textbox=tk.Text(self.Frame, font=('Courier New',12),
                        undo=True, maxundo=10, wrap=tk.NONE,
                        tabs=40, xscrollcommand=BarX.set, yscrollcommand=BarY.set)
        BarX.config(command=Textbox.xview)
        BarY.config(command=Textbox.yview)
        TextFile=open(filepath, "rt")
        Textbox.insert("end", TextFile.read())
        TextFile.close()
        def ButtonCommand():
            self.Unhighlight()
            self.Highlight(self.Current.get())
        Textbutton=tk.Radiobutton(self.Frame, text=filename, command=ButtonCommand, font=('Courier New',10),
                                  variable=self.Current, value=self.Len, indicatoron=False)
        Textbutton.place(relx=1/self.ButtonNum*self.Len, rely=0.0, relwidth=1/self.ButtonNum, relheight=self.ButtonRelHeight)
        self.Text.append((Textbox, BarX, BarY, Textbutton, filepath))
        self.Current.set(self.Len)
        self.Len+=1
        ButtonCommand()
        
    def Highlight(self, index): #切换到第index个编辑界面
        if (index >= self.Len or index < 0):
            return
        self.Highlighted=index
        self.Text[index][0].place(relx=0.0, rely=self.ButtonRelHeight, relwidth=1.0-self.ScrollbarRelWidth, relheight=1.0-self.ButtonRelHeight-self.ScrollbarRelWidth/self.Height*self.Width)
        self.Text[index][1].place(relx=0.0, rely=1.0-self.ScrollbarRelWidth/self.Height*self.Width, relwidth=1.0-self.ScrollbarRelWidth, relheight=self.ScrollbarRelWidth/self.Height*self.Width)
        self.Text[index][2].place(relx=1.0-self.ScrollbarRelWidth, rely=self.ButtonRelHeight, relwidth=self.ScrollbarRelWidth, relheight=1.0-self.ButtonRelHeight)  
    def Unhighlight(self): #隐藏当前编辑界面
        if (self.Highlighted == -1):
            return
        i=self.Highlighted
        self.Highlighted=-1
        self.Text[i][0].place_forget()
        self.Text[i][1].place_forget()
        self.Text[i][2].place_forget()
    def Savefile(self): #保存当前文件
        if (self.Highlighted == -1):
            return
        TextFile=open(self.Text[self.Highlighted][4], "wt")
        TextFile.write(self.Text[self.Highlighted][0].get("1.0", tk.END))
        TextFile.close()
    def Closefile(self): #关闭当前文件
        if (self.Highlighted == -1):
            return
        self.Len-=1
        tup=self.Text.pop(self.Highlighted)
        for i in range(len(tup)-1):
            tup[i].destroy()
        for i in range(self.Highlighted, self.Len):
            self.Text[i][3]["value"]=i
            self.Text[i][3].place(relx=1/self.ButtonNum*i, rely=0.0, relwidth=1/self.ButtonNum, relheight=self.ButtonRelHeight)
        self.Highlighted=-1
        self.Current.set(self.Len-1)
        self.Highlight(self.Len-1)
    def Newfile(self): #新建文件并打开
        filepath=tkfile.asksaveasfilename(title=u'新建SQL文件',filetypes=[('SQL文件','.sql')])
        if (filepath == ""):
            return
        if (len(filepath)<4 or filepath[-4:]!=".sql"):
            filepath+=".sql"
        TextFile=open(filepath, "wt")
        TextFile.close()
        self.Openfile(filepath=filepath)
    def GetCommand(self): #返回当前编辑窗口选中的指令，如果没有选中部分，则返回整个窗口所有的指令
        if (self.Highlighted == -1):
            return None
        try:
            return self.Text[self.Highlighted][0].selection_get()
        except:
            return self.Text[self.Highlighted][0].get("1.0", tk.END)
    def ExecuteCommand(self, cmd=None): #执行指令
        global Outputbox
        global Resultbox
        global Schemasbox
        if(cmd is None):
            cmd=self.GetCommand().split(";")
        for item in cmd:
            item=re.sub(" +", " ", item.replace("\n", " ").replace("\t", " ")).strip()+";"
            #过滤空语句
            if(item == ";"):
                continue
            dll.Exec(item.encode("utf-8"))
            Outputbox.Insert(dll.GetState(), item, dll.GetMessage().decode("utf-8"), dll.GetTime()/1000)
            #execfile文件, 更新模式栏
            if (len(item)>=8 and item[0:8]=="execfile"):
                Schemasbox.Update()
            #执行失败, 立刻退出
            if(dll.GetState() == 0):
                break
            #判断是否退出
            if(dll.Quit()):
                pass
            #判断查询结果
            Resultbox.Update(dll.GetCounter())
            if (len(item)>=10 and item[0:10]=="drop table") or (len(item)>=12 and item[0:12]=="create table"):
                Schemasbox.Update()



#查询结果栏, 显示上一次查询结果
class ResultFrame:
    def __init__(self, Height=800, Width=200, ButtonNum=10, ButtonRelHeight=0.08, ScrollbarRelWidth=0.015):
        self.Height=Height #查询结果栏高度
        self.Width=Width   #查询结果栏宽度
        self.ButtonNum=ButtonNum #最多同时打开的查询结果个数
        self.HistoryCnt=-1 #最晚的一次查询编号
        self.Len=0 #当前查询个数, =Head-Tail+1
        self.ButtonRelHeight=ButtonRelHeight #查询结果标签相对高度
        self.ScrollbarRelWidth=ScrollbarRelWidth #滚动轴相对宽度
        self.Frame=tk.Frame(root, background=FrameColor)
        self.Query=[] #(Treeview, BarX, BarY, Button)
        self.Highlighted=-1
        self.Current=tk.IntVar()
        self.Current.set(-1)
    def Update(self, Counter):
        if(Counter == self.HistoryCnt):
            return
        while(self.Len > 0 and self.ButtonNum - self.Len < Counter - self.HistoryCnt):
            self.Highlight(0)
            self.Closequery()
        for i in range(self.HistoryCnt+1, Counter+1):
            fin=open("./Result/Result"+str(i)+".txt","r")
            self.Newquery(eval(fin.readline()))
            line=fin.readline()
            while(line):
                self.InsertTuple(eval(line))
                line=fin.readline()
            fin.close()
    def Newquery(self, headings): #新建一个查询结果(只添加表的标题，内容暂不添加)
        if (self.Len >= self.ButtonNum):
            self.Highlight(0)
            self.Closequery()
        self.HistoryCnt+=1
        BarX=tk.Scrollbar(self.Frame, orient=tk.HORIZONTAL)
        BarY=tk.Scrollbar(self.Frame, orient=tk.VERTICAL)
        Querybox=ttk.Treeview(self.Frame, columns=headings,
                              show="headings", xscrollcommand=BarX.set, yscrollcommand=BarY.set)
        for item in headings:
            Querybox.heading(item, text=item)
        BarX.config(command=Querybox.xview)
        BarY.config(command=Querybox.yview)
        def ButtonCommand():
            self.Unhighlight()
            self.Highlight(self.Current.get())
        Querybutton=tk.Radiobutton(self.Frame, text="Query "+str(self.HistoryCnt), command=ButtonCommand,
                                   font=('Courier New',10), variable=self.Current, value=self.Len, indicatoron=False)
        Querybutton.place(relx=1/self.ButtonNum*self.Len, rely=0.0, relwidth=1/self.ButtonNum, relheight=self.ButtonRelHeight)
        self.Query.append((Querybox, BarX, BarY, Querybutton))
        self.Current.set(self.Len)
        self.Len+=1
        ButtonCommand()
    def Highlight(self, index=None): #切换到第index个查询结果
        if (index is None):
            index = self.Len - 1
        if (index >= self.Len or index < 0):
            return
        self.Highlighted=index
        self.Query[index][0].place(relx=0.0, rely=self.ButtonRelHeight, relwidth=1.0-self.ScrollbarRelWidth, relheight=1.0-self.ButtonRelHeight-self.ScrollbarRelWidth/self.Height*self.Width)
        self.Query[index][1].place(relx=0.0, rely=1.0-self.ScrollbarRelWidth/self.Height*self.Width, relwidth=1.0-self.ScrollbarRelWidth, relheight=self.ScrollbarRelWidth/self.Height*self.Width)
        self.Query[index][2].place(relx=1.0-self.ScrollbarRelWidth, rely=self.ButtonRelHeight, relwidth=self.ScrollbarRelWidth, relheight=1.0-self.ButtonRelHeight)  
    def Unhighlight(self): #隐藏当前查询结果
        if (self.Highlighted == -1):
            return
        i=self.Highlighted
        self.Highlighted=-1
        self.Query[i][0].place_forget()
        self.Query[i][1].place_forget()
        self.Query[i][2].place_forget()
    def InsertTuple(self, content, index=None): #向某个查询结果表格内添加元组
        if (index is None):
            index=self.Len-1
        elif (index >= self.Len):
            return
        self.Query[index][0].insert("", "end", values=content)   
    def Closequery(self): #删除当前查询结果表格
        if (self.Highlighted == -1):
            return
        self.Len-=1
        tup=self.Query.pop(self.Highlighted)
        for i in range(len(tup)):
            tup[i].destroy()
        for i in range(self.Highlighted, self.Len):
            self.Query[i][3]["value"]=i
            self.Query[i][3].place(relx=1/self.ButtonNum*i, rely=0.0, relwidth=1/self.ButtonNum, relheight=self.ButtonRelHeight)
        self.Highlighted=-1
        self.Current.set(self.Len-1)
        self.Highlight(self.Len-1)
    def Clearquery(self): #清空所有查询结果
        for i in range(self.Len):
            tup=self.Query[i]
            for j in range(len(tup)):
                tup[j].destroy()
        self.Highlighted=-1
        self.Current.set(-1)
        self.Len=0
        self.Query=[]
        
        
        
        
#输出栏，显示上一次指令执行的返回信息
#Treeview+右侧水平滚动轴
#Treeview每行包含5条信息，分别是：成功/失败，编号，执行内容，返回信息，用时
class OutputFrame:
    def __init__(self, Height=400, Width=1000):
        self.Height=Height #输出栏高度
        self.Width=Width #输出栏宽度
        self.Len=0 #输出栏已有的信息数
        self.Frame=tk.Frame(root, background=FrameColor)
        self.Scrollbar=tk.Scrollbar(self.Frame, orient=tk.VERTICAL)
        self.MessageTable=ttk.Treeview(self.Frame, columns=("State", "#", "Action", "Message", "Duration"),
                                       show="headings", yscrollcommand=self.Scrollbar.set)
        self.Scrollbar.config(command=self.MessageTable.yview)
        self.MessageTable.column("State", width=int(self.Width*0.01))
        self.MessageTable.column("#", width=int(self.Width*0.03))
        self.MessageTable.column("Action", width=int(self.Width*0.4))
        self.MessageTable.column("Message", width=int(self.Width*0.4))
        self.MessageTable.column("Duration", width=int(self.Width*0.11))
        self.MessageTable.heading("State", text=" ")
        self.MessageTable.heading("#", text="#")
        self.MessageTable.heading("Action", text="Action")
        self.MessageTable.heading("Message", text="Message")
        self.MessageTable.heading("Duration", text="Duration")
        self.MessageTable.place(relx=0.0, rely=0.0, relwidth=0.98, relheight=1.0)
        self.Scrollbar.place(relx=0.98, rely=0.0, relwidth=0.015, relheight=1.0)
    def Insert(self, state, action, message, duration): #插入一条输出信息
        self.Len+=1
        self.MessageTable.insert("", self.Len, values=("√" if (state) else "×", str(self.Len), action, message, str(duration)+"SEC"))
        
        
        
        
#工具栏
class ToolFrame:
    def __init__(self, Height=80, Width=1000, ButtonNumX=8, ButtonNumY=1, MarginX=0.01, MarginY=0.01):
        self.Height=Height
        self.Width=Width
        self.ButtonNumX=ButtonNumX
        self.ButtonNumY=ButtonNumY
        self.MarginX=MarginX
        self.MarginY=MarginY
        self.Frame=tk.Frame(root, background=FrameColor)
        self.B=[[None for i in range(ButtonNumX)] for j in range(ButtonNumY)]
    def CreateButton(self, FrameList):
        ButtonNumX=self.ButtonNumX
        ButtonNumY=self.ButtonNumY
        MarginX=self.MarginX
        MarginY=self.MarginY
        Codeframe=FrameList[0]
        Queryframe=FrameList[1]
        Schemasframe=FrameList[2]
        Outputframe=FrameList[3]
        self.B[0][0]=tk.Button(self.Frame, text="New File", command=Codeframe.Newfile, font=('Arial',12))
        self.B[0][1]=tk.Button(self.Frame, text="Open File", command=Codeframe.Openfile, font=('Arial',12))
        self.B[0][2]=tk.Button(self.Frame, text="Execute", command=Codeframe.ExecuteCommand, font=('Arial',12))
        self.B[0][3]=tk.Button(self.Frame, text="Save File", command=Codeframe.Savefile, font=('Arial',12))
        self.B[0][4]=tk.Button(self.Frame, text="Close File", command=Codeframe.Closefile, font=('Arial',12))
        self.B[0][5]=tk.Button(self.Frame, text="Close Query", command=Queryframe.Closequery, font=('Arial',12))
        self.B[0][6]=tk.Button(self.Frame, text="Delete Schema", command=Schemasframe.DeleteSchema, font=('Arial',12))
        self.B[0][7]=tk.Button(self.Frame, text="Exit", command=Exit, font=('Arial',12))
        for j in range(len(self.B)):
            for i in range(len(self.B[j])):
                if (self.B[j][i] is None):
                    continue
                self.B[j][i].place(relx=MarginX*(i+1)+(1.0-MarginX*(ButtonNumX+1))/ButtonNumX*i,
                                   rely=MarginY*(j+1)+(1.0-MarginY*(ButtonNumY+1))/ButtonNumY*j,
                                   relwidth=(1.0-MarginX*(ButtonNumX+1))/ButtonNumX,
                                   relheight=(1.0-MarginY*(ButtonNumY+1))/ButtonNumY)

                
                
#关系模式栏
#由Listbox和垂直滚动轴组成
class SchemasFrame:
    def __init__(self, Height=400, Width=50, ScrollbarRelWidth=0.1):
        self.Height=Height
        self.Width=Width
        self.ScrollbarRelWidth=ScrollbarRelWidth
        self.Frame=tk.Frame(root, background=FrameColor)
        self.Bar=tk.Scrollbar(self.Frame, orient=tk.VERTICAL)
        self.List=tk.Listbox(self.Frame, selectmode='single', yscrollcommand=self.Bar.set)
        self.Bar.config(command=self.List.yview)
        self.List.place(relx=0.0, rely=0.0, relwidth=1.0-ScrollbarRelWidth, relheight=1.0)
        self.Bar.place(relx=1.0-ScrollbarRelWidth, rely=0.0, relwidth=ScrollbarRelWidth, relheight=1.0)
        self.Update()
    def Update(self):
        self.List.delete(0, tk.END)
        for root, dirs, files in os.walk("./Catalog/"):
            if(root=="./Catalog/"):
                for item in dirs:
                    self.List.insert(tk.END, item)
                break
    def DeleteSchema(self):
        global Codebox
        for item in self.List.curselection()[::-1]:
            Codebox.ExecuteCommand(["drop table "+self.List.get(item)])




#主窗口
root=tk.Tk()
root.configure(background=RootColor)
root.title("miniSQL")
root.geometry("{}x{}+0+0".format(root.winfo_screenwidth(), root.winfo_screenheight()-70))
root.update()
root.resizable(False, False)
RootWidth=root.winfo_width()
RootHeight=root.winfo_height()
MarginY=10
MarginX=10
root.protocol("WM_DELETE_WINDOW", Exit) #检测窗口关闭事件，关闭前先要logout

#工具栏
Toolbox=ToolFrame(Height=30,Width=RootWidth-MarginX*2)
#执行输出栏
Outputbox=OutputFrame(Height=150, Width=RootWidth-MarginX*2)
Outputbox.Frame.place(x=(RootWidth-Outputbox.Width)//2,
                      y=RootHeight-Outputbox.Height-MarginY,
                      width=Outputbox.Width, height=Outputbox.Height)
#关系模式栏
Schemasbox=SchemasFrame(Height=RootHeight-MarginY*4-Toolbox.Height-Outputbox.Height, Width=200)
Schemasbox.Frame.place(x=MarginX, y=Toolbox.Height+MarginY*2,
                       width=Schemasbox.Width, height=Schemasbox.Height)
#查询结果栏
Resultbox=ResultFrame(Height=300, Width=RootWidth-MarginX*3-Schemasbox.Width)
Resultbox.Frame.place(x=MarginX*2+Schemasbox.Width, y=RootHeight-Outputbox.Height-MarginY*2-Resultbox.Height,
                      width=Resultbox.Width, height=Resultbox.Height)
#代码编辑栏
Codebox=CodeFrame(Height=RootHeight-MarginY*5-Toolbox.Height-Outputbox.Height-Resultbox.Height, Width=RootWidth-MarginX*3-Schemasbox.Width)
Codebox.Frame.place(x=MarginX*2+Schemasbox.Width, y=Toolbox.Height+2*MarginY,
                    width=Codebox.Width, height=Codebox.Height)
#创建工具栏按钮
Toolbox.CreateButton([Codebox, Resultbox, Schemasbox, Outputbox])
Toolbox.Frame.place(x=(RootWidth-Toolbox.Width)//2, y=MarginY,
                    width=Toolbox.Width, height=Toolbox.Height)
root.mainloop()