#     nano-csc-tcc-win-linux-ftp-in-120lines
A zero-dependency, 120-line single-file C# (Windows) or C (Linux) FTP server. Compiles on any Windows PC using the internal CSC compiler or any Linux using tcc. Optimized for Beyond Compare and Windows Explorer.
 * The C# code compiles on ANY Windows PC using ONLY the internal Windows C# compiler.
 * Build: C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /out:cs_ftp.exe cs_ftp.cs
 * Usage: cs_ftp.exe [root_directory]
 * NOTE: Use forward slashes for paths (e.g., cs_ftp.exe C:/)
 --------  
 * The C code compiles on ANY Linux PC using tcc or the internal gcc compiler.
 * Build: tcc -o tcc_ftp tcc_ftp.c
 * Usage: ./tcc_ftp [root] [optional_ip_override_for_Chromebook]
 * NOTE: if your Linux does't have 200KB tcc you SHOULD get it: "sudo apt install tcc"
--------------------------------------------------------
 * LICENSE: MIT (Free, no-strings-attached)
 * ORIGIN: Human + Gemini 3 Flash AI
   
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal 
 in the Software without restriction, including without limitation the rights 
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
--------------------------------------------------------
5-Second Windows Build instruction

        Copy source code cs_ftp.cs to your computer.

        Open CMD and run:
        C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /out:cs_ftp.exe cs_ftp.cs

        You now have your own trusted binary!

5-Second Linux Build instruction

        Copy source code tcc_ftp.c to your computer.

        Open Terminal and run:
        tcc -o tcc_ftp tcc_ftp.c

        You now have your own trusted binary!
