namespace Tester {
    partial class MainForm {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing) {
            if (disposing && (components != null)) {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent() {
            this.Run1 = new System.Windows.Forms.Button();
            this.Display = new System.Windows.Forms.Label();
            this.Run2 = new System.Windows.Forms.Button();
            this.Run3 = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // Run1
            // 
            this.Run1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(24)))), ((int)(((byte)(24)))), ((int)(((byte)(24)))));
            this.Run1.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.Run1.Location = new System.Drawing.Point(15, 12);
            this.Run1.Name = "Run1";
            this.Run1.Size = new System.Drawing.Size(48, 27);
            this.Run1.TabIndex = 0;
            this.Run1.Text = "Run 1";
            this.Run1.UseVisualStyleBackColor = false;
            this.Run1.Click += new System.EventHandler(this.Run1_Click);
            // 
            // Display
            // 
            this.Display.AutoSize = true;
            this.Display.Location = new System.Drawing.Point(178, 19);
            this.Display.Name = "Display";
            this.Display.Size = new System.Drawing.Size(28, 13);
            this.Display.TabIndex = 1;
            this.Display.Text = "Text";
            // 
            // Run2
            // 
            this.Run2.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(24)))), ((int)(((byte)(24)))), ((int)(((byte)(24)))));
            this.Run2.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.Run2.Location = new System.Drawing.Point(70, 12);
            this.Run2.Name = "Run2";
            this.Run2.Size = new System.Drawing.Size(48, 27);
            this.Run2.TabIndex = 0;
            this.Run2.Text = "Run 2";
            this.Run2.UseVisualStyleBackColor = false;
            this.Run2.Click += new System.EventHandler(this.Run2_Click);
            // 
            // Run3
            // 
            this.Run3.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(24)))), ((int)(((byte)(24)))), ((int)(((byte)(24)))));
            this.Run3.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.Run3.Location = new System.Drawing.Point(124, 12);
            this.Run3.Name = "Run3";
            this.Run3.Size = new System.Drawing.Size(48, 27);
            this.Run3.TabIndex = 2;
            this.Run3.Text = "Run 3";
            this.Run3.UseVisualStyleBackColor = false;
            this.Run3.Click += new System.EventHandler(this.Run3_Click);
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(16)))), ((int)(((byte)(16)))), ((int)(((byte)(16)))));
            this.ClientSize = new System.Drawing.Size(928, 50);
            this.Controls.Add(this.Run3);
            this.Controls.Add(this.Display);
            this.Controls.Add(this.Run2);
            this.Controls.Add(this.Run1);
            this.ForeColor = System.Drawing.Color.Gainsboro;
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "MainForm";
            this.Text = "PerfectClear Tester";
            this.Load += new System.EventHandler(this.MainForm_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button Run1;
        private System.Windows.Forms.Label Display;
        private System.Windows.Forms.Button Run2;
        private System.Windows.Forms.Button Run3;
    }
}