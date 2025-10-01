<h1>My Fileserver from Scratch</h1>
<p>I began working on this project after reading Beej's Guide to Network programming - it is my first "real" personal project.<p>

<p>Although I know that file transfer servers have been done a million times I wanted to build a Linux fileserver from scratch and really get my hands dirty with syscalls, 
    sockets, error handling, debugging C code, header files, and so on.<p>

<p> The goals for this project are as follows:</p>
<ol>
      <li> Be able to transfer MOST (if not all) kinds of files</li>
      <li> Allow for connecting clients to specify a user to access the fileserver with</li>
      <li> Allow the fileserver user to be able to see the contents of their fileserver directory</li>
      <li> The ability to also download files from the fileserver</li>
      <li> A log file tied to the server program to troubleshoot if problems</li>
      <li> A progress bar that tells the client the progress of their transfer / download</li>
      <li> Multiple clients able to connect simultaneously</li>
      <li> Turn this into a packaged service that runs cleanly on Linux and allows for tuning of different variables with a .conf file</li>
</ol>

<h1>References</h1>
<p>I will gradually update my references as I wrap up different goals for this project.<p>
