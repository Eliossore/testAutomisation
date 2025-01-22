using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.IO;
using static EcoflocConfigurator.TextBoxHelper;

namespace EcoflocConfigurator
{
    public class NICInfo
    {
        public int upload_rate { get; set; }
        public int download_rate { get; set; }
        public int max_upload_rate { get; set; }
        public int max_download_rate { get; set; }
    }
    /// <summary>
    /// Logique d'interaction pour NICView.xaml
    /// </summary>
    public partial class NICView : UserControl
    {
        NICInfo info;
        public NICView()
        {
            InitializeComponent();
            info = new NICInfo();

            try
            {
                if (System.IO.File.Exists("./config/nic.json"))
                {
                    string textInfo = System.IO.File.ReadAllText("./config/nic.json");
                    if (!string.IsNullOrEmpty(textInfo))
                    {
                        info = System.Text.Json.JsonSerializer.Deserialize<NICInfo>(textInfo);
                    }

                    UploadPower_TextBox.Foreground = Brushes.Black;
                    UploadPower_TextBox.Text = info.upload_rate.ToString();
                    DownloadPower_TextBox.Foreground = Brushes.Black;
                    DownloadPower_TextBox.Text = info.download_rate.ToString();
                    MaxDownloadRate_TextBox.Foreground = Brushes.Black;
                    MaxDownloadRate_TextBox.Text = info.max_download_rate.ToString();
                    MaxUploadRate_TextBox.Foreground = Brushes.Black;
                    MaxUploadRate_TextBox.Text = info.max_upload_rate.ToString();
                }
            }
            catch (IOException)
            {
                // Gérer l'exception sans afficher de MessageBox
                // Vous pouvez enregistrer l'erreur dans un journal ou simplement l'ignorer
            }
        }

        private void FloatOnly_PreviewTextInput(object sender, TextCompositionEventArgs e)
        {
            // Use a regular expression to allow only numbers with an optional single decimal point
            e.Handled = !IsValidFloatInput(((TextBox)sender).Text, e.Text);
        }

        private void FloatOnly_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            // Allow special keys like Backspace, Delete, Tab, Left, Right, etc.
            if (e.Key == Key.Back || e.Key == Key.Delete || e.Key == Key.Tab || e.Key == Key.Left || e.Key == Key.Right)
            {
                e.Handled = false;
            }
        }

        // Method to check if the input is a valid float pattern
        private bool IsValidFloatInput(string currentText, string newText)
        {
            string fullText = currentText + newText;

            // Regular expression pattern: start with an optional "-" sign, followed by digits, optional decimal point, and more digits
            string pattern = @"^-?\d*\.?\d*$";

            // Match against the full text input
            return Regex.IsMatch(fullText, pattern);
        }

        private void FloatOnly_Pasting(object sender, DataObjectPastingEventArgs e)
        {
            if (e.DataObject.GetDataPresent(typeof(string)))
            {
                string pastedText = (string)e.DataObject.GetData(typeof(string));
                TextBox textBox = sender as TextBox;

                // Check if the pasted content is a valid float
                if (!IsValidFloatInput(textBox.Text, pastedText))
                {
                    e.CancelCommand(); // Cancel the paste if it's not valid
                }
            }
            else
            {
                e.CancelCommand(); // Cancel if the data is not string type
            }
        }

        private void InfoButton_Click(object sender, RoutedEventArgs e)
        {
        }

        public void SaveButton_Click(object sender, RoutedEventArgs e)
        {
            if (HasEmptyField())
            {
                MessageBox.Show("Please fill in all the fields", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }

            // Save the CPU information
            info.upload_rate = int.Parse(UploadPower_TextBox.Text);
            info.download_rate = int.Parse(DownloadPower_TextBox.Text);
            info.max_upload_rate = int.Parse(MaxDownloadRate_TextBox.Text);
            info.max_download_rate = int.Parse(MaxUploadRate_TextBox.Text);

            // Save the information as JSON
            string json = System.Text.Json.JsonSerializer.Serialize(info);
            System.IO.File.WriteAllText("./config/nic.json", json);
        }

        private bool IsTextBoxEmpty(TextBox textBox)
        {
            return string.IsNullOrEmpty(textBox.Text) || textBox.Text == GetHint(textBox);
        }


        public bool HasAllFieldEmpty()
        {
            return IsTextBoxEmpty(UploadPower_TextBox) && IsTextBoxEmpty(DownloadPower_TextBox) && IsTextBoxEmpty(MaxDownloadRate_TextBox) && IsTextBoxEmpty(MaxUploadRate_TextBox);
        }

        public bool HasEmptyField()
        {
            return IsTextBoxEmpty(UploadPower_TextBox) || IsTextBoxEmpty(DownloadPower_TextBox) || IsTextBoxEmpty(MaxDownloadRate_TextBox) || IsTextBoxEmpty(MaxUploadRate_TextBox);
        }

        public bool HasUnsavedChanges()
        {
            return info.upload_rate != int.Parse(UploadPower_TextBox.Text) || info.download_rate != int.Parse(DownloadPower_TextBox.Text) || info.max_download_rate != int.Parse(MaxDownloadRate_TextBox.Text) || info.max_upload_rate != int.Parse(MaxUploadRate_TextBox.Text);
        }
    }
}
