using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Windows.Forms;
using System.Windows.Media.Imaging;

namespace styleGANapp
{
    public partial class Form1 : Form
    {
        public string appPath = "";
        public string stylegan_path = "";
        public string curDir = "";

        int image_count = 0;

        public Form1()
        {
            InitializeComponent();

            string path =
                 System.Reflection.Assembly.GetExecutingAssembly().GetName().CodeBase;
            Uri u = new Uri(path);
            path = u.LocalPath + Uri.UnescapeDataString(u.Fragment);
            appPath = System.IO.Path.GetDirectoryName(path);

            StreamReader sr = null;
            try
            {
                path = appPath+"\\stylegan_path.txt";
                sr = new StreamReader(path);
                string line = null;
                while ((line = sr.ReadLine()) != null)
                {
                    stylegan_path = line.Replace("\n", "");
                    break;
                }
            }
            catch (Exception e)
            {
                string message = e.Message;
                MessageBox.Show(message);
            }
            curDir = System.IO.Path.GetDirectoryName(stylegan_path + "\\..\\");
        }
        public static System.Drawing.Image CreateImage(string filename)
        {
            System.IO.FileStream fs = new System.IO.FileStream(
                filename,
                System.IO.FileMode.Open,
                System.IO.FileAccess.Read);
            System.Drawing.Image img = System.Drawing.Image.FromStream(fs);
            fs.Close();
            return img;
        }
        private void ImageClear()
        {
            for (int i = 0; i < 100000; i++)
            {
                string image = "image_" + String.Format("{0:D4}", i) + ".png";
                if (File.Exists(image))
                {
                    File.Delete(image);
                }
                else
                {
                    break;
                }
            }
        }

        private void p_Exited(object sender, EventArgs e)
        {
            MessageBox.Show("終了しました。");
            timer1.Stop();
            timer1.Enabled = false;
        }

        private void label3_Click(object sender, EventArgs e)
        {

        }

        private void numericUpDown3_ValueChanged(object sender, EventArgs e)
        {
            if ( numericUpDown3.Value > 1)
            {
                trackBar1.Enabled = true;
                progressBar1.Enabled = true;
                button2.Enabled = true;
            }else
            {
                trackBar1.Enabled = false;
                progressBar1.Enabled = false;
                button2.Enabled = false;
            }
        }

        private void button1_Click(object sender, EventArgs e)
        {
            var app = new ProcessStartInfo();
            app.FileName = stylegan_path + "\\stylegan.exe";
            app.Arguments = "";

            app.Arguments += " --seed " + numericUpDown1.Value.ToString();
            app.Arguments += " --seed2 " + numericUpDown2.Value.ToString();
            app.Arguments += " --num " + numericUpDown3.Value.ToString();
            app.Arguments += " --start_index " + numericUpDown4.Value.ToString();

            if (checkBox3.Checked)
            {
                app.Arguments += " --random_seed 1";
            }
            if ( checkBox1.Checked)
            {
                app.Arguments += " --smooth_z 1";
            }
            if (checkBox2.Checked)
            {
                app.Arguments += " --smooth_psi 1";
            }

            string model = "StyleGAN_karras2019stylegan-ffhq-1024x1024.ct4";
            if ( comboBox1.Text == "FFHQ Faces")
            {
                model = "StyleGAN_karras2019stylegan-ffhq-1024x1024.ct4";
            }
            if (comboBox1.Text == "CelebA HQ Faces")
            {
                model = "StyleGAN_karras2019stylegan-celebahq-1024x1024.ct4";
            }
            if (comboBox1.Text == "FFHQ Faces")
            {
                model = "StyleGAN_karras2019stylegan-celebahq-1024x1024.ct4";
            }
            if (comboBox1.Text == "Anime Faces1")
            {
                model = "StyleGAN_2019-03-08-stylegan-animefaces-network-02051-021980.ct4";
            }
            if (comboBox1.Text == "Anime Faces2")
            {
                model = "StyleGAN_2019-02-26-stylegan-faces-network-02048-016041.ct4";
            }
            if (comboBox1.Text == "Anime Portraits")
            {
                model = "StyleGAN_2019-04-30-stylegan-danbooru2018-portraits-02095-066083.ct4";
            }
            app.Arguments += " --model "+ model;

            app.Arguments += " --model_path " + System.IO.Path.GetDirectoryName(stylegan_path + "\\..\\model\\")+"/";


            if ( checkBox1.Checked || checkBox2.Checked || checkBox3.Checked)
            {
                if (numericUpDown3.Value < 2)
                {
                    MessageBox.Show("生成数が少ないため変更しました");
                    numericUpDown3.Value = 10;
                }
            }

            trackBar1.Value = 0;
            trackBar1.Minimum = 0;
            progressBar1.Minimum = 0;
            progressBar1.Value = 0;
            progressBar1.Enabled = false;
            trackBar1.Enabled = false;
            if (numericUpDown3.Value > 1)
            {
                progressBar1.Enabled = true;
                progressBar1.Maximum = (int)numericUpDown3.Value;

                trackBar1.Enabled = true;
                trackBar1.Maximum = (int)numericUpDown3.Value;
            }

            string now_curdir = System.IO.Directory.GetCurrentDirectory();
            System.IO.Directory.SetCurrentDirectory(curDir);

            Encoding sjisEnc = Encoding.GetEncoding("Shift_JIS");
            using (StreamWriter writer = new StreamWriter("command_line.txt", false, sjisEnc))
            {
                writer.WriteLine(DateTime.Now.ToString("HH:mm:ss") + app.FileName + " " + app.Arguments);
            }

            app.UseShellExecute = false;

            System.Diagnostics.Process p = new System.Diagnostics.Process();
            p.StartInfo = app;

            
            //p.SynchronizingObject = this;
            p.Exited += new EventHandler(p_Exited);
            p.EnableRaisingEvents = true;

            image_count = 0;
            ImageClear();
            timer1.Enabled = true;
            timer1.Start();
            p.Start();

            //p.WaitForExit();
            
            //System.IO.Directory.SetCurrentDirectory(now_curdir);
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            if (image_count == numericUpDown3.Value)
                return;

            string image = curDir + "\\image_" + String.Format("{0:D4}", image_count) + ".png";
            if ( File.Exists(image))
            {
                try
                {
                    pictureBox1.Image = CreateImage(image);
                }catch
                {
                    return;
                }
                image_count++;
                progressBar1.Value = image_count;
            }
            pictureBox1.Refresh();
        }

        private void checkBox3_CheckedChanged(object sender, EventArgs e)
        {
            if ( checkBox3.Checked)
            {
                checkBox1.Enabled = false;
                checkBox2.Enabled = false;
                numericUpDown1.Enabled = false;
                numericUpDown2.Enabled = false;
            }else
            {
                checkBox1.Enabled = true;
                checkBox2.Enabled = true;
                numericUpDown1.Enabled = true;
                numericUpDown2.Enabled = true;
            }
        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            if ( checkBox1.Checked)
            {
                numericUpDown2.Enabled = true;
                checkBox2.Enabled = false;
                checkBox3.Enabled = false;
            }else
            {
                numericUpDown2.Enabled = false;
                checkBox2.Enabled = true;
                checkBox3.Enabled = true;
            }
        }

        private void trackBar1_Scroll(object sender, EventArgs e)
        {
            string image = curDir + "\\image_" + String.Format("{0:D4}", trackBar1.Value) + ".png";
            if (File.Exists(image))
            {
                pictureBox1.Image = CreateImage(image);
            }
            pictureBox1.Refresh();
        }

        public void CreateAnimatedGif(string savePath)
        {
            GifBitmapEncoder encoder = new GifBitmapEncoder();

            for (int i = 0; i < numericUpDown3.Value; i++)
            {
                string image = curDir + "\\image_" + String.Format("{0:D4}", i) + ".png";
                if (File.Exists(image))
                {
                    BitmapFrame bmpFrame =
                        BitmapFrame.Create(new Uri(image, UriKind.RelativeOrAbsolute));
                    encoder.Frames.Add(bmpFrame);
                }
            }

            FileStream outputFileStrm = new FileStream(savePath,
                FileMode.Create, FileAccess.Write, FileShare.None);
            encoder.Save(outputFileStrm);
            outputFileStrm.Close();
        }
        private void button2_Click(object sender, EventArgs e)
        {
            CreateAnimatedGif("animation.gif");
        }

        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            if ( checkBox2.Checked)
            {
                checkBox1.Enabled = false;
                checkBox3.Enabled = false;
            }else
            {
                checkBox1.Enabled = true;
                checkBox3.Enabled = true;
            }
        }
    }
}
