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
  
Your app should now be online.  
  
Your app logic is in silgy_app.cpp and app_process_req() is your main, called with every browser request.
