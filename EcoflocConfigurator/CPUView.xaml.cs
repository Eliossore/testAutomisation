using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
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
using System.Text.RegularExpressions;
using static EcoflocConfigurator.TextBoxHelper;
using System.IO;

namespace EcoflocConfigurator
{
    public class CPUInfo
    {
        public int tdp { get; set; }
        public int clockSpeed { get; set; }
        public float voltage { get; set; }
    }
    /// <summary>
    /// Logique d'interaction pour CPUView.xaml
    /// </summary>
    public partial class CPUView : UserControl
    {
        CPUInfo info;
        public CPUView()
        {
            InitializeComponent();
            info = new CPUInfo();
            try
            {
                if (System.IO.File.Exists("./config/cpu.json"))
                {
                    string textInfo = System.IO.File.ReadAllText("./config/cpu.json");
                    if (!string.IsNullOrEmpty(textInfo))
                    {
                        info = System.Text.Json.JsonSerializer.Deserialize<CPUInfo>(textInfo);
                    }

                    TDPTextBox.Foreground = Brushes.Black;
                    TDPTextBox.Text = info.tdp.ToString();
                    ClockSpeedTextBox.Foreground = Brushes.Black;
                    ClockSpeedTextBox.Text = info.clockSpeed.ToString();
                    VoltageTextBox.Foreground = Brushes.Black;
                    VoltageTextBox.Text = info.voltage.ToString().Replace(",", ".");
                }
            }
            catch (IOException)
            {
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
            info.tdp = int.Parse(TDPTextBox.Text);
            info.clockSpeed = int.Parse(ClockSpeedTextBox.Text);
            info.voltage = float.Parse(VoltageTextBox.Text.Replace(".", ","));

            // Save the information as JSON
            string json = System.Text.Json.JsonSerializer.Serialize(info);
            System.IO.File.WriteAllText("./config/cpu.json", json);
        }

        private bool IsTextBoxEmpty(TextBox textBox)
        {
            return string.IsNullOrEmpty(textBox.Text) || textBox.Text == GetHint(textBox);
        }

        public bool HasAllFieldEmpty()
        {
            return IsTextBoxEmpty(TDPTextBox) && IsTextBoxEmpty(ClockSpeedTextBox) && IsTextBoxEmpty(VoltageTextBox);
        }

        public bool HasEmptyField()
        {
            return IsTextBoxEmpty(TDPTextBox) || IsTextBoxEmpty(ClockSpeedTextBox) || IsTextBoxEmpty(VoltageTextBox);
        }

        public bool HasUnsavedChanges()
        {
            return info.tdp != int.Parse(TDPTextBox.Text) || info.clockSpeed != int.Parse(ClockSpeedTextBox.Text) || info.voltage != float.Parse(VoltageTextBox.Text.Replace(".", ","));
        }
    }
}
