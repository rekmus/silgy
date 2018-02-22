# silgy
Ultra-fast asynchronous web engine that allows you to compile and link your logic into one executable.  
  
Silgy requires Linux/UNIX computer with C or C++ compiler.  
  
Quick Start Guide:  
  
Create a project directory, i.e. web:  
mkdir web  
cd web  
mkdir src  
mkdir bin  
mkdir res  
mkdir resmin  
mkdir logs  
  
Then copy all the files to src. m, mobj and te must have executable flag. In src:  
chmod u+x m*  
chmod u+x te  
  
Set SILGYDIR environment variable to your project directory.  
  
Go to src and compile everything:  
mobj  
Make your executable:  
m  
Run:  
sudo te  
In the browser, navigate to your host.
