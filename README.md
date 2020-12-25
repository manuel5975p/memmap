# memmap
A header-only cross-platform C++17 library for memory-mapping files  
  
Boost I/O Performance with `<memmap.hpp>` instead of `<fstream>`  

### Usage example
```cpp
#include <memmap.hpp>
#include <iostream>
int main(){
  filemap<true> fmr("file1.txt");   //Read-only access
                                    //Same as const_filemap
  
  filemap<false> fmrw("file2.txt"); //Read-Write access
  
  //fmr.resize(1024); <- Error, resizing read-only file!
  std::cout << fmr.size() << "\n";
  
  fmrw.resize(1024); //Truncates or extends the file
  std::cout << fmrw.size() << "\n";
  
  for(int i = 0;i < fmrw.size();i++){
    fmrw[i] = 'a' + (i % 26); //Changing data in read-write mode
  }
  
  std::for_each(fmr.begin(), fmr.end(), [](const char& x){/*...*/}); 
  std::generate(fmrw.begin(), fmrw.end(), []{/*...*/});
  
  fmr.close();  //Destructors would also do the job;
  fmrw.close(); //Calling close() is optional here.
                 
}
```
