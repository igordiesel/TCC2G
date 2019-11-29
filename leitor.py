import serial
import time
from tkinter import *
from threading import Thread,Lock
import os
from datetime import datetime as dt

class Reader(Thread):
        def __init__ (self,com,folder,print_time,close_function):
                Thread.__init__(self)
                self.print_time = print_time
                self.folder = folder
                self.com = com
                self.exit = close_function
                self.flag = True 
                self.mutex = Lock()
                
        def run(self):
                init_time = time.time()
                value = 0
                low_readed = False
                file_opened = False

                zero_count = 0
                found_markers = 0 

                self.mutex.acquire()
                while(self.flag):
                        if self.com.in_waiting:
                                lido = self.com.read()
                                self.file.write(lido) # Problema: talvez tenha que fazer uma conversão para string, não sei se aceita byte
                                if int.from_bytes(,"big") == 0:
                                    zero_count += 1
                                    if zero_count >= 4:
                                        found_markers += 1
                                        if found_markers >= 5:
                                            self.exit()
                                else:
                                    zero_cont = 0
                self.mutex.release()

        def play(self):
                self.flag = True

        def pause(self):
                print('Interrompendo thread')
                mutex = Lock()
                mutex.acquire()
                self.flag = False
                mutex.release()

class Interface:
        def __init__(self, master):
                self.master = master
                master.title("DataLogger")
                
                self.label0 = Label(master, text="Nome do arquivo de saída:")
                self.label0.pack()
                
                self.e_file = Entry(master)
                self.e_file.insert(0,"Data")
                self.e_file.pack()
                
                self.label1 = Label(master, text="Porta Serial:")
                self.label1.pack()
                
                self.e_port = Entry(master)
                self.e_port.insert(0,"COM3")
                self.e_port.pack()
                
                self.label2 = Label(master, text="Baud Rate:")
                self.label2.pack()
                
                self.e_baud = Entry(master)
                self.e_baud.insert(0,"9600")
                self.e_baud.pack()
                
                self.com = serial.Serial()

                self.print_time = IntVar()
                Checkbutton(master, text="Imprimir tempo", variable=self.print_time).pack()
                
                self.greet_button = Button(master, text="Open", command=self.open)
                self.greet_button.pack()

                self.close_button = Button(master, text="Close", command=self.close,state=DISABLED)
                self.close_button.pack()

        def read(self):
                self.reader.play()
                #print(self.com.read())
        
        def close(self):
                try:
                        self.reader.pause()
                        self.reader.join()		
                except:
                        print('Erro ao pausar Leitor')
                try:
                        self.com.close()		
                except:
                        print('Erro ao fechar porta serial')
                try:
                        self.file.close()		
                except:
                        print('Erro ao fechar arquivo')
                self.greet_button['state']=NORMAL
                self.close_button['state']=DISABLED

        def exit(self):
                self.close()
                self.master.destroy()

        def open(self):
                self.com.baudrate = int(self.e_baud.get())
                self.com.port = self.e_port.get()
                self.com.open()
                print("Porta aberta! "+str(self.com.is_open))

                if not os.path.exists(self.e_file.get()):
                        os.makedirs(self.e_file.get())
                print("Folder criado! ")
                
                self.reader = Reader(self.com,self.e_file.get(),self.print_time,self.exit)
                self.reader.start()
                self.greet_button['state']=DISABLED
                self.close_button['state']=NORMAL

root = Tk()

my_gui = Interface(root)
root.protocol("WM_DELETE_WINDOW", my_gui.exit)
root.mainloop()

