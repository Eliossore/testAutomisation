using System;
using System.IO;
using System.Windows;
using System.Threading.Tasks;

namespace EcoflocConfigurator
{
    public partial class App : Application
    {
        protected override async void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            // Afficher le répertoire de travail pour le débogage
            //Console.WriteLine("Répertoire de travail initial : " + Directory.GetCurrentDirectory());

            // Changer explicitement le répertoire de travail
            string currentDirectory = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
            Directory.SetCurrentDirectory(currentDirectory);

            //Console.WriteLine("Répertoire de travail défini : " + Directory.GetCurrentDirectory());

            // Délai pour permettre à l'application de se mettre au premier plan
            await Task.Delay(1000);

            // Mettre la fenêtre au premier plan et lui donner le focus
            if (MainWindow != null)
            {
                //MainWindow.Activate();
                //MainWindow.Focus();
                MainWindow.Topmost = true;
                MainWindow.Topmost = false;  // Désactiver Topmost après pour ne pas forcer la fenêtre à rester toujours au-dessus
            }
        }
    }
}