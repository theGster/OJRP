//setting up our namespace stuff.
using System;
using System.Windows.Forms;
using System.Drawing;
using System.IO;


namespace Merger
{
	/// <summary>
	/// The form class for our primary program window.
	/// </summary>
    public class mainWindow : Form
    {
        Button baseBrowse, addBrowse, outBrowse, mergeButton;
        TextBox baseFile, addFile, outFile;
        Label baseFile_Label, addFile_Label, outFile_Label;
        ToolTip toolTips;

        //static window size
        private System.ComponentModel.IContainer components;
        private CheckBox addRoot;

        animation_t animationList = null;

        public mainWindow()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(mainWindow));
            this.toolTips = new System.Windows.Forms.ToolTip(this.components);
            this.baseFile_Label = new System.Windows.Forms.Label();
            this.baseBrowse = new System.Windows.Forms.Button();
            this.addFile_Label = new System.Windows.Forms.Label();
            this.addBrowse = new System.Windows.Forms.Button();
            this.outFile_Label = new System.Windows.Forms.Label();
            this.outBrowse = new System.Windows.Forms.Button();
            this.mergeButton = new System.Windows.Forms.Button();
            this.addRoot = new System.Windows.Forms.CheckBox();
            this.baseFile = new System.Windows.Forms.TextBox();
            this.addFile = new System.Windows.Forms.TextBox();
            this.outFile = new System.Windows.Forms.TextBox();
            this.SuspendLayout();
            // 
            // baseFile_Label
            // 
            this.baseFile_Label.AutoSize = true;
            this.baseFile_Label.Location = new System.Drawing.Point(3, 23);
            this.baseFile_Label.Name = "baseFile_Label";
            this.baseFile_Label.Size = new System.Drawing.Size(63, 13);
            this.baseFile_Label.TabIndex = 9;
            this.baseFile_Label.Text = "Primary File:";
            this.toolTips.SetToolTip(this.baseFile_Label, "The primary file\'s data will be placed at the beginning of the output file.");
            // 
            // baseBrowse
            // 
            this.baseBrowse.Location = new System.Drawing.Point(270, 18);
            this.baseBrowse.Name = "baseBrowse";
            this.baseBrowse.Size = new System.Drawing.Size(75, 23);
            this.baseBrowse.TabIndex = 10;
            this.baseBrowse.Text = "Browse";
            this.toolTips.SetToolTip(this.baseBrowse, "Browse for Primary File.");
            this.baseBrowse.Click += new System.EventHandler(this.ActionBaseBrowse);
            // 
            // addFile_Label
            // 
            this.addFile_Label.AutoSize = true;
            this.addFile_Label.Location = new System.Drawing.Point(3, 55);
            this.addFile_Label.Name = "addFile_Label";
            this.addFile_Label.Size = new System.Drawing.Size(80, 13);
            this.addFile_Label.TabIndex = 11;
            this.addFile_Label.Text = "Secondary File:";
            this.toolTips.SetToolTip(this.addFile_Label, "The secondary file\'s data will be placed at the end of the output file.");
            // 
            // addBrowse
            // 
            this.addBrowse.Location = new System.Drawing.Point(270, 50);
            this.addBrowse.Name = "addBrowse";
            this.addBrowse.Size = new System.Drawing.Size(75, 23);
            this.addBrowse.TabIndex = 12;
            this.addBrowse.Text = "Browse";
            this.toolTips.SetToolTip(this.addBrowse, "Browse for Secondary File.");
            this.addBrowse.Click += new System.EventHandler(this.ActionAddBrowse);
            // 
            // outFile_Label
            // 
            this.outFile_Label.AutoSize = true;
            this.outFile_Label.Location = new System.Drawing.Point(3, 105);
            this.outFile_Label.Name = "outFile_Label";
            this.outFile_Label.Size = new System.Drawing.Size(61, 13);
            this.outFile_Label.TabIndex = 13;
            this.outFile_Label.Text = "Output File:";
            this.toolTips.SetToolTip(this.outFile_Label, "All data will be outputed to this file.");
            // 
            // outBrowse
            // 
            this.outBrowse.Location = new System.Drawing.Point(270, 100);
            this.outBrowse.Name = "outBrowse";
            this.outBrowse.Size = new System.Drawing.Size(75, 23);
            this.outBrowse.TabIndex = 14;
            this.outBrowse.Text = "Browse";
            this.toolTips.SetToolTip(this.outBrowse, "Browse for Output File Location.");
            this.outBrowse.Click += new System.EventHandler(this.ActionOutBrowse);
            // 
            // mergeButton
            // 
            this.mergeButton.Enabled = false;
            this.mergeButton.Location = new System.Drawing.Point(360, 18);
            this.mergeButton.Name = "mergeButton";
            this.mergeButton.Size = new System.Drawing.Size(105, 105);
            this.mergeButton.TabIndex = 15;
            this.mergeButton.Text = "Merge";
            this.toolTips.SetToolTip(this.mergeButton, "Merge animation.cfg files.");
            this.mergeButton.Click += new System.EventHandler(this.ActionMergeCfgs);
            // 
            // addRoot
            // 
            this.addRoot.AutoSize = true;
            this.addRoot.Location = new System.Drawing.Point(221, 77);
            this.addRoot.Name = "addRoot";
            this.addRoot.Size = new System.Drawing.Size(124, 17);
            this.addRoot.TabIndex = 16;
            this.addRoot.Text = "Remove Root Frame";
            this.toolTips.SetToolTip(this.addRoot, resources.GetString("addRoot.ToolTip"));
            this.addRoot.UseVisualStyleBackColor = true;
            // 
            // baseFile
            // 
            this.baseFile.Location = new System.Drawing.Point(85, 20);
            this.baseFile.Name = "baseFile";
            this.baseFile.Size = new System.Drawing.Size(180, 20);
            this.baseFile.TabIndex = 8;
            this.baseFile.TextChanged += new System.EventHandler(this.ChangedFileTexts);
            // 
            // addFile
            // 
            this.addFile.Location = new System.Drawing.Point(85, 52);
            this.addFile.Name = "addFile";
            this.addFile.Size = new System.Drawing.Size(180, 20);
            this.addFile.TabIndex = 8;
            this.addFile.TextChanged += new System.EventHandler(this.ChangedFileTexts);
            // 
            // outFile
            // 
            this.outFile.Location = new System.Drawing.Point(85, 102);
            this.outFile.Name = "outFile";
            this.outFile.Size = new System.Drawing.Size(180, 20);
            this.outFile.TabIndex = 8;
            this.outFile.TextChanged += new System.EventHandler(this.ChangedFileTexts);
            // 
            // mainWindow
            // 
            this.ClientSize = new System.Drawing.Size(492, 148);
            this.Controls.Add(this.addRoot);
            this.Controls.Add(this.baseFile);
            this.Controls.Add(this.baseFile_Label);
            this.Controls.Add(this.baseBrowse);
            this.Controls.Add(this.addFile);
            this.Controls.Add(this.addFile_Label);
            this.Controls.Add(this.addBrowse);
            this.Controls.Add(this.outFile);
            this.Controls.Add(this.outFile_Label);
            this.Controls.Add(this.outBrowse);
            this.Controls.Add(this.mergeButton);
            this.MaximizeBox = false;
            this.MaximumSize = new System.Drawing.Size(500, 175);
            this.MinimizeBox = false;
            this.MinimumSize = new System.Drawing.Size(500, 175);
            this.Name = "mainWindow";
            this.Text = "GLA animation.cfg Merger $Revision: 1.2 $";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        private void ActionMergeCfgs(object obj, EventArgs ea)
        {
            FileStream baseFileStream, addFileStream, outFileStream;
            try
            {
                baseFileStream = new FileStream(baseFile.Text, FileMode.Open, FileAccess.Read);
            }
            catch (FileNotFoundException e)
            {//bad base file
                MessageBox.Show(e.Message, "Primary File Load Error");
                return;
            }

            try
            {
                addFileStream = new FileStream(addFile.Text, FileMode.Open, FileAccess.Read);
            }
            catch (FileNotFoundException e)
            {//bad add file
                MessageBox.Show(e.Message, "Secondary File Load Error");
                return;
            }

            try
            {
                outFileStream = new FileStream(outFile.Text, FileMode.Create, FileAccess.Write);
            }
            catch (Exception e)
            {//bad add file
                MessageBox.Show(e.Message, "Output File Save Error");
                return;
            }

            StreamReader baseIn = new StreamReader(baseFileStream);
            LoadAnimationsFromFile(baseIn, baseFile.Text, 0);
            baseIn.Close();

            //determine the frame offset for the additional file.
            int frameOffset = 0;
            animation_t currentAnim = animationList;
            if (currentAnim == null)
            {
                MessageBox.Show("No animation data was found in the primary file!\nContinuing to secondary file.", "Merging Error");
            }
            else
            {

                for (; currentAnim != null; currentAnim = currentAnim.nextAnim)
                {
                    if (!currentAnim.isComment)
                    {
                        int currentAnimOffset = currentAnim.startFrame + currentAnim.frameCount - 1;
                        if (currentAnimOffset >= frameOffset)
                        {
                            frameOffset = currentAnimOffset + 1;
                        }
                    }
                }

                if (frameOffset == 0)
                {
                    MessageBox.Show("No animation frames used in the primary file!\nContinuing to secondary file.", "Merging Error");
                }

                //adjusting for root frame removal.  
                //This occurs if the user uses the -o option for glamerge and there was a root frame
                if (addRoot.Checked)
                {
                    frameOffset--;
                }
            }

            StreamReader addIn = new StreamReader(addFileStream);
            LoadAnimationsFromFile(addIn, addFile.Text, frameOffset);
            addIn.Close();

            StreamWriter output = new StreamWriter(outFileStream);
            SaveAnimationsToFile(output);

            //delete the animation list
            animationList = null;

            MessageBox.Show("Successfully merged animation.cfg files.");
        }

        private void LoadAnimationsFromFile(StreamReader inStream, String fileName, int frameOffset)
        {
            int lineNum = 0;
            String currentLine;
            for (; (currentLine = inStream.ReadLine()) != null; lineNum++ )
            {
                if (currentLine.StartsWith("//"))
                {//this line was commented out
                    AddAnimation(currentLine);
                }
                else
                {//animation line
                    char[] separators = { ' ', '\t' };
                    String[] animTokens = currentLine.Split(separators, StringSplitOptions.RemoveEmptyEntries);
                    if (animTokens.Length > 5)
                    {
                        MessageBox.Show("Too many items on line " + lineNum + " of\n" + fileName
                                + ".\nThe proper formating of a animation.cfg is....\n"
                                + "AnimationName\tStartingFrame\tNumberofFraces\tLoopingFrame\tFrameSpeed\n"
                                + "Moving onto next line.",
                                "Merging Error!");
                        continue;
                    }
                    else if (animTokens.Length < 5)
                    {
                        MessageBox.Show("Too few items on line " + lineNum + " of\n" + fileName
                                + ".\nThe proper formating of a animation.cfg is....\n"
                                + "AnimationName\tStartingFrame\tNumberofFraces\tLoopingFrame\tFrameSpeed\n"
                                + "Moving onto next line.",
                                "Merging Error!");
                        continue;
                    }

                    AddAnimation(animTokens[0], Convert.ToInt32(animTokens[1]) + frameOffset, 
                        Convert.ToInt32(animTokens[2]), animTokens[3], animTokens[4]);
                }

                
            }
        }

        /// <summary>
        /// Add an line as a commented animation line.
        /// </summary>
        /// <param name="name"></param>
        private void AddAnimation(String name)
        {
            animation_t newAnim;
            newAnim = new animation_t();
            newAnim.Name = name;
            newAnim.isComment = true;
            newAnim.nextAnim = null;

            AddAnimationToStack(newAnim);
        }

        private void AddAnimation(String name, int startFrame, int frameCount, String loopFrame, String frameSpeed)
        {
            animation_t newAnim;
            newAnim = new animation_t();
            newAnim.Name = name;
            newAnim.startFrame = startFrame;
            newAnim.frameCount = frameCount;
            newAnim.loopFrame = loopFrame;
            newAnim.frameSpeed = frameSpeed;
            newAnim.isComment = false;
            newAnim.nextAnim = null;

            AddAnimationToStack(newAnim);
        }

        private void AddAnimationToStack(animation_t newAnim)
        {
            if (animationList == null)
            {//first animation
                animationList = newAnim;
            }
            else
            {
                animation_t currentList = animationList;
                while (currentList.nextAnim != null)
                {
                    currentList = currentList.nextAnim;
                }

                currentList.nextAnim = newAnim;
            }
        }

        private void SaveAnimationsToFile(StreamWriter outStream)
        {
            for (animation_t currentAnimation = animationList; 
                currentAnimation != null; currentAnimation = currentAnimation.nextAnim)
            {
                if (currentAnimation.isComment)
                {
                    outStream.WriteLine(currentAnimation.Name);
                }
                else
                {
                    outStream.WriteLine("{0,-37}\t{1}\t{2}\t{3}\t{4}", 
                        currentAnimation.Name,
                        currentAnimation.startFrame, 
                        currentAnimation.frameCount, 
                        currentAnimation.loopFrame, 
                        currentAnimation.frameSpeed);
                }
            }

            outStream.Close();
        }

        private void ActionBaseBrowse(object obj, EventArgs ea)
        {
            SetInFileBrowse(baseFile, "Set Primary File");
        }

        private void ActionAddBrowse(object obj, EventArgs ea)
        {
            SetInFileBrowse(addFile, "Set Secondary File");
        }

        private void ActionOutBrowse(object obj, EventArgs ea)
        {
            SetOutFileBrowse(outFile, "Set Output File");
        }

        private void SetOutFileBrowse(TextBox parentBox, String title)
        {
            SaveFileDialog saveCfgFile;
            saveCfgFile = new SaveFileDialog();
            saveCfgFile.Filter = "GLA Animation Configuration File (*.cfg)|*.cfg|All Files|*.*";
            saveCfgFile.FilterIndex = 1;
            saveCfgFile.Title = title;
            saveCfgFile.DefaultExt = ".cfg";

            if (saveCfgFile.ShowDialog() == DialogResult.OK)
            {
                parentBox.Text = saveCfgFile.FileName;
            }
        }

        private void SetInFileBrowse(TextBox parentBox, String title)
        {
            OpenFileDialog openCfgFile;
            openCfgFile = new OpenFileDialog();
            openCfgFile.Filter = "GLA Animation Configuration File (*.cfg)|*.cfg|All Files|*.*";
            openCfgFile.FilterIndex = 1;
            openCfgFile.Title = title;
            openCfgFile.DefaultExt = ".cfg";

            if (openCfgFile.ShowDialog() == DialogResult.OK)
            {
                parentBox.Text = openCfgFile.FileName;
            }
        }

        private void ChangedFileTexts(object obj, EventArgs ea)
        {
            if (!String.IsNullOrEmpty(outFile.Text) && !String.IsNullOrEmpty(addFile.Text) 
                && !String.IsNullOrEmpty(outFile.Text))
            {//all files have been entered
                mergeButton.Enabled = true;
            }
        }

        [STAThread]
        private static void Main()
        {//this bad boy is our program entry point.
            mainWindow programWindow;
            programWindow = new mainWindow();
            Application.Run(programWindow);
        }
    }

    public class animation_t
    {
        public String Name;
        public int startFrame;
        public int frameCount;
        public String loopFrame;
        public String frameSpeed;
        public Boolean isComment;

        public animation_t nextAnim;
    }
}
