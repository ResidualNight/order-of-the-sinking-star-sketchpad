using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Management.Automation;
using System.Management.Automation.Runspaces;

internal static class SetupHost
{
    [STAThread]
    private static int Main(string[] args)
    {
        Application.EnableVisualStyles();
        try
        {
            string script = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "StarSketch-Setup.ps1");
            if (!File.Exists(script)) throw new FileNotFoundException("[SS-INSTALL-037] Please extract the complete StarSketch installer ZIP before running it.", script);
            InitialSessionState settings = InitialSessionState.CreateDefault();
            // This policy belongs to this in-process runspace; no machine/user setting is changed.
            settings.ExecutionPolicy = Microsoft.PowerShell.ExecutionPolicy.Bypass;
            using (Runspace runspace = RunspaceFactory.CreateRunspace(settings))
            {
                runspace.ApartmentState = ApartmentState.STA;
                runspace.ThreadOptions = PSThreadOptions.UseCurrentThread;
                runspace.Open();
                using (PowerShell shell = PowerShell.Create())
                {
                    shell.Runspace = runspace;
                    shell.AddCommand(script);
                    if (args.Length > 0) shell.AddParameter("GameDirectory", args[0]);
                    shell.Invoke();
                    if (shell.HadErrors)
                    {
                        StringBuilder message = new StringBuilder();
                        foreach (ErrorRecord error in shell.Streams.Error) message.AppendLine(error.ToString());
                        throw new InvalidOperationException(message.ToString());
                    }
                }
            }
            return 0;
        }
        catch (Exception error)
        {
            string message = error.Message;
            if (!System.Text.RegularExpressions.Regex.IsMatch(message, @"^\[SS-(INSTALL|UPDATE)-\d{3}\] ")) message = "[SS-INSTALL-098] " + message;
            MessageBox.Show(message, "StarSketch", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}
