using System.IO;
using System.Windows;

namespace EcoflocConfigurator
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private const string ConfigDirectory = "./config";

        public MainWindow()
        {
            InitializeComponent();

            // Créer le dossier config si nécessaire
            EnsureConfigDirectoryExists();

            // Charger la vue par défaut (CPUView)
            MainContentControl.Content = new CPUView();
            CPUButton.IsEnabled = false;
        }

        private void EnsureConfigDirectoryExists()
        {
            // Vérifie si le dossier config existe, sinon le crée
            if (!Directory.Exists(ConfigDirectory))
            {
                Directory.CreateDirectory(ConfigDirectory);
            }
        }

        private void MainWindows_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (!CheckForUnsavedChanges())
            {
                e.Cancel = true;
            }
        }

        private void CPUButton_Click(object sender, RoutedEventArgs e)
        {
            NavigateToView(ViewType.CPU);
        }

        private void SDButton_Click(object sender, RoutedEventArgs e)
        {
            NavigateToView(ViewType.SD);
        }

        private void NICButton_Click(object sender, RoutedEventArgs e)
        {
            NavigateToView(ViewType.NIC);
        }

        private void NavigateToView(ViewType viewType)
        {
            if (!CheckForEmptyField())
            {
                return;
            }

            if (!CheckForUnsavedChanges())
            {
                return;
            }

            switch (viewType)
            {
                case ViewType.CPU:
                    MainContentControl.Content = new CPUView();
                    CPUButton.IsEnabled = false;
                    SDButton.IsEnabled = true;
                    NICButton.IsEnabled = true;
                    break;
                case ViewType.SD:
                    MainContentControl.Content = new SDView();
                    CPUButton.IsEnabled = true;
                    SDButton.IsEnabled = false;
                    NICButton.IsEnabled = true;
                    break;
                case ViewType.NIC:
                    MainContentControl.Content = new NICView();
                    CPUButton.IsEnabled = true;
                    SDButton.IsEnabled = true;
                    NICButton.IsEnabled = false;
                    break;
            }
        }

        private bool CheckForUnsavedChanges()
        {
            if (MainContentControl.Content is CPUView cpuView)
            {
                if (cpuView.HasAllFieldEmpty())
                {
                    return true;
                }
                if (cpuView.HasUnsavedChanges())
                {
                    MessageBoxResult result = MessageBox.Show("You have unsaved changes. Do you want to save them before switching?", "Unsaved Changes", MessageBoxButton.YesNoCancel, MessageBoxImage.Warning);
                    if (result == MessageBoxResult.Yes)
                    {
                        cpuView.SaveButton_Click(null, null);
                        return true;
                    }
                    else if (result == MessageBoxResult.No)
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
                return true;
            }
            return true;
        }

        private bool CheckForEmptyField()
        {
            switch (MainContentControl.Content)
            {
                case CPUView cpuView:
                    if (cpuView.HasAllFieldEmpty())
                    {
                        return true;
                    }
                    if (cpuView.HasEmptyField())
                    {
                        MessageBoxResult result = MessageBox.Show("Please fill in all the fields", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        return result != MessageBoxResult.OK;
                    }
                    break;

                case SDView sdView:
                    if (sdView.HasAllFieldEmpty())
                    {
                        return true;
                    }
                    if (sdView.HasEmptyField())
                    {
                        MessageBoxResult result = MessageBox.Show("Please fill in all the fields", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        return result != MessageBoxResult.OK;
                    }
                    break;

                case NICView nicView:
                    if (nicView.HasAllFieldEmpty())
                    {
                        return true;
                    }
                    if (nicView.HasEmptyField())
                    {
                        MessageBoxResult result = MessageBox.Show("Please fill in all the fields", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        return result != MessageBoxResult.OK;
                    }
                    break;
            }
            return true;
        }
    }
}
