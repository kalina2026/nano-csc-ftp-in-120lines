/*
 * Portable (120 lines) True Open Source C# FTP Server
 * --------------------------------------------------------
 * This code compiles on ANY Windows PC using ONLY the internal Windows C# compiler.
 * Build: C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /out:cs_ftp.exe cs_ftp.cs
 * Usage: cs_ftp.exe [root_directory]
 * NOTE: Use forward slashes for paths (e.g., cs_ftp.exe C:/) 
 * --------------------------------------------------------
 * LICENSE: MIT (Free, no-strings-attached)
 * ORIGIN: Human + Gemini 3 Flash AI
 * * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies.
 * * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 * --------------------------------------------------------
 */
using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

class PortableFtpServer {
    static string auth = "vpn:vpn", user_part, pass_part, root_dir = "", global_ip_comma = "127,0,0,1";
    static int CTRL_PORT = 2121, DATA_PORT = 2122;
    static Socket data_listener = null;

    static void Main(string[] args) {
        user_part = auth.Split(':')[0]; pass_part = auth.Split(':')[1];
        root_dir = args.Length > 0 ? Path.GetFullPath(args[0].TrimEnd('\\')) : AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
        Directory.SetCurrentDirectory(root_dir);

        Console.CancelKeyPress += (s, e) => { Console.WriteLine("\nStopping..."); Environment.Exit(0); };

        try {
            using (Socket s = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, 0)) {
                s.Connect("8.8.8.8", 53);
                global_ip_comma = ((IPEndPoint)s.LocalEndPoint).Address.ToString().Replace('.', ',');
            }
        } catch { }
        Socket ls = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        ls.Bind(new IPEndPoint(IPAddress.Any, CTRL_PORT));
        ls.Listen(10);

        Console.WriteLine("Root: {0}", root_dir);
        Console.WriteLine("URL:  ftp://{0}:{1}@{2}:{3}", user_part, pass_part, global_ip_comma.Replace(',', '.'), CTRL_PORT);
        Console.WriteLine("Press [Ctrl+C] to stop.");

        while (true) { try { Socket cs = ls.Accept(); new Thread(() => ProcessClient(cs)).Start(); } catch { } }
    }
    static void ProcessClient(Socket cs) {
        cs.ReceiveTimeout = 60000;
        string cur = "/", rn = ""; byte[] buf = new byte[1024];
        send_s(cs, "220 Ready\r\n");
        try {
            int n;
            while ((n = cs.Receive(buf)) > 0) {
                string s = Encoding.ASCII.GetString(buf, 0, n);
                string c = s.Length >= 3 ? s.Substring(0, 4).Trim().ToUpper() : "";
                string p = s.Length > 4 ? s.Substring(s.IndexOf(' ') + 1).Trim() : "";
                //if (c != "PASS") Console.WriteLine("-> {0} {1}", c, p);

                if (c == "USER") send_s(cs, s.Contains(user_part) ? "331 OK\r\n" : "530 Fail\r\n");
                else if (c == "PASS") send_s(cs, s.Contains(pass_part) ? "230 OK\r\n" : "530 Fail\r\n");
                else if (c == "FEAT") send_s(cs, "211-Extensions:\r\n MLSD\r\n MDTM\r\n MFMT\r\n UTF8\r\n211 End\r\n");
                else if (c == "PWD" || c == "XPWD") send_s(cs, "257 \"" + cur + "\"\r\n");
                else if (c == "TYPE") send_s(cs, "200 OK\r\n");
                else if (c == "PASV") {
                    if (data_listener != null) data_listener.Close();
                    data_listener = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                    data_listener.Bind(new IPEndPoint(IPAddress.Any, DATA_PORT));
                    data_listener.Listen(1);
                    send_s(cs, string.Format("227 Entering Passive Mode ({0},8,74).\r\n", global_ip_comma));
                }
                else if (c == "LIST" || c == "MLSD") { send_s(cs, "150 OK\r\n"); send_list(cur, c == "MLSD"); send_s(cs, "226 OK\r\n"); }
                else if (c == "CWD") {
                    string t = p.Replace('\\', '/'), trg = t.StartsWith("/") ? t : (cur.TrimEnd('/') + "/" + t);
                    if (t == "..") { int l = cur.TrimEnd('/').LastIndexOf('/'); trg = l < 0 ? "/" : cur.Substring(0, l); }
                    if (Directory.Exists(get_lp(trg, ""))) { cur = trg == "" ? "/" : trg; send_s(cs, "250 OK\r\n"); } else send_s(cs, "550 Fail\r\n");
                }
                else if (c == "MKD" || c == "XMKD") { try { Directory.CreateDirectory(get_lp(cur, p)); send_s(cs, "257 OK\r\n"); } catch { send_s(cs, "550 Fail\r\n"); } }
                else if (c == "DELE" || c == "RMD") { try { string lp = get_lp(cur, p); if (c == "DELE") File.Delete(lp); else Directory.Delete(lp, true); send_s(cs, "250 OK\r\n"); } catch { send_s(cs, "550 Fail\r\n"); } }
                else if (c == "RNFR") { rn = p; send_s(cs, "350 OK\r\n"); }
                else if (c == "RNTO") { try { string p1 = get_lp(cur, rn), p2 = get_lp(cur, p); if (File.Exists(p1)) File.Move(p1, p2); else Directory.Move(p1, p2); send_s(cs, "250 OK\r\n"); } catch { send_s(cs, "550 Fail\r\n"); } }
                else if (c == "MDTM") { string lp = get_lp(cur, p); if (File.Exists(lp)) send_s(cs, "213 " + File.GetLastWriteTime(lp).ToUniversalTime().ToString("yyyyMMddHHmmss") + "\r\n"); else send_s(cs, "550 Fail\r\n"); }
                else if (c == "MFMT") {
                    try {
                        string ts = p.Substring(0, 14), fn = p.Substring(15).Trim(), lp = get_lp(cur, fn);
                        DateTime dt = new DateTime(int.Parse(ts.Substring(0, 4)), int.Parse(ts.Substring(4, 2)), int.Parse(ts.Substring(6, 2)), int.Parse(ts.Substring(8, 2)), int.Parse(ts.Substring(10, 2)), int.Parse(ts.Substring(12, 2)), DateTimeKind.Utc);
                        File.SetLastWriteTime(lp, dt.ToLocalTime()); send_s(cs, "213 Modify=" + ts + "; " + fn + "\r\n");
                    } catch { send_s(cs, "550 Fail\r\n"); }
                }
                else if (c == "RETR" || c == "STOR") { send_s(cs, "150 OK\r\n"); if (file_op(cur, p, c == "RETR" ? 1 : 0) != 0) send_s(cs, "226 OK\r\n"); else send_s(cs, "550 Fail\r\n"); }
                else if (c == "QUIT") { send_s(cs, "221 Bye\r\n"); break; }
                else send_s(cs, "200 OK\r\n");
                Array.Clear(buf, 0, 1024);
            }
        } catch { } finally { cs.Close(); }
    }
    static string get_lp(string c, string f) {
        string p = Path.GetFullPath(Path.Combine(root_dir, c.TrimStart('/').Replace('/', '\\'), f));
        return p.StartsWith(root_dir, StringComparison.OrdinalIgnoreCase) ? p : root_dir;
    }
    static void send_s(Socket s, string m) { try { s.Send(Encoding.ASCII.GetBytes(m)); } catch { } }
    static void send_list(string v, bool m) {
        if (data_listener == null) return;
        try {
            using (Socket cl = data_listener.Accept()) {
                string l = get_lp(v, "");
                foreach (string e in Directory.GetFileSystemEntries(l)) {
                    bool d = Directory.Exists(e); FileSystemInfo i = d ? (FileSystemInfo)new DirectoryInfo(e) : new FileInfo(e);
                    if (i.Name.StartsWith(".")) continue;
                    string n;
                    if (m) n = string.Format("modify={0};type={1};size={2}; {3}\r\n", i.LastWriteTime.ToUniversalTime().ToString("yyyyMMddHHmmss"), d ? "dir" : "file", d ? 0 : ((FileInfo)i).Length, i.Name);
                    else n = string.Format("{0} {1} {2}\r\n", i.LastWriteTime.ToString("MM-dd-yy  hh:mmtt"), d ? "<DIR>          " : ((FileInfo)i).Length.ToString().PadLeft(14), i.Name);
                    cl.Send(Encoding.ASCII.GetBytes(n));
                }
            }
        } catch { } finally { if (data_listener != null) { data_listener.Close(); data_listener = null; } }
    }
    static int file_op(string v, string f, int r) {
        if (data_listener == null) return 0;
        try {
            using (Socket cl = data_listener.Accept()) {
                string lp = get_lp(v, f);
                if (r != 0) using (FileStream fs = File.OpenRead(lp)) { byte[] b = new byte[8192]; int n; while ((n = fs.Read(b, 0, b.Length)) > 0) cl.Send(b, 0, n, 0); return 1; }
                else using (FileStream fs = File.Create(lp)) { byte[] b = new byte[8192]; int n; while ((n = cl.Receive(b)) > 0) fs.Write(b, 0, n); return 1; }
            }
        } catch { return 0; } finally { if (data_listener != null) { data_listener.Close(); data_listener = null; } }
    }
}