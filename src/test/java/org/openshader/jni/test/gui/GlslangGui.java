package org.openshader.jni.test.gui;

import java.awt.EventQueue;

import javax.swing.JFileChooser;
import javax.swing.JFrame;
import java.awt.BorderLayout;
import javax.swing.JMenuBar;
import javax.swing.JSeparator;
import javax.swing.SwingConstants;
import javax.swing.UIManager;
import javax.swing.filechooser.FileSystemView;

import org.openshader.jni.NativeLibraryException;
import org.openshader.jni.OpenShaderJni;
import org.openshader.jni.compiler.CompileJob;
import org.openshader.jni.compiler.IHeaderIncluder;
import org.openshader.jni.compiler.ShaderCompiler;
import org.openshader.jni.compiler.ShaderCompiler.ShaderType;
import org.openshader.jni.compiler.ShaderCompilerManager;

import javax.swing.JSplitPane;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.JMenu;
import javax.swing.JMenuItem;
import java.awt.event.ActionListener;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.awt.event.ActionEvent;

public class GlslangGui {

	private JFrame frmGlslangTest;

	/**
	 * Launch the application.
	 */
	public static void main(String[] args) throws Exception {
		OpenShaderJni.init();
		EventQueue.invokeLater(new Runnable() {
			public void run() {
				try {
					UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
					GlslangGui window = new GlslangGui();
					window.frmGlslangTest.setVisible(true);
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		});
	}

	/**
	 * Create the application.
	 */
	public GlslangGui() {
		initialize();
	}

	/**
	 * Initialize the contents of the frame.
	 */
	private void initialize() {
		frmGlslangTest = new JFrame();
		frmGlslangTest.setTitle("Glslang Test");
		frmGlslangTest.setBounds(100, 100, 800, 600);
		frmGlslangTest.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
		frmGlslangTest.getContentPane().setLayout(new BorderLayout(0, 0));
		
		JSplitPane splitPane = new JSplitPane();
		splitPane.setResizeWeight(0.5);
		frmGlslangTest.getContentPane().add(splitPane, BorderLayout.CENTER);
		
		JScrollPane scrollPane = new JScrollPane();
		splitPane.setLeftComponent(scrollPane);
		
		JTextArea textArea = new JTextArea();
		scrollPane.setViewportView(textArea);
		
		JScrollPane scrollPane_1 = new JScrollPane();
		splitPane.setRightComponent(scrollPane_1);
		
		JTextArea textArea_1 = new JTextArea();
		scrollPane_1.setViewportView(textArea_1);
		
		JMenuBar menuBar = new JMenuBar();
		frmGlslangTest.setJMenuBar(menuBar);
		
		JMenu mnFile = new JMenu("File");
		menuBar.add(mnFile);
		
		JMenuItem mntmLoad = new JMenuItem("Load");
		mntmLoad.addActionListener(new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				JFileChooser chooser = new JFileChooser();
				int result = chooser.showOpenDialog(frmGlslangTest);
				if (result == JFileChooser.APPROVE_OPTION) {
					StringBuilder sb = new StringBuilder();
					BufferedReader br = null;
					String input;
					try {
						br = new BufferedReader(new FileReader(chooser.getSelectedFile()));
						while ((input = br.readLine()) != null)
						{
							sb.append(input).append("\n");
						}
					} catch (Exception ex) {
						ex.printStackTrace();
						return;
					} finally {
						if (br != null)
							try { br.close(); } catch (IOException e1) {}
					}
					input = sb.toString();
					textArea.setText(input);
					String fileName = chooser.getSelectedFile().getName();
					ShaderType type = ShaderType.VERTEX;
					if (fileName.endsWith(".frag") || fileName.endsWith(".fsh"))
						type = ShaderType.FRAGMENT;
					
					ShaderCompiler compiler = ShaderCompilerManager.createShaderCompiler(120);
					compiler.setIncluder(new IHeaderIncluder() {
						@Override
						public String includeSystem(String headerName, String includerName) {
							return "int " + headerName + " = 1;";
						}
						
						@Override
						public String includeLocal(String headerName, String includerName) {
							return "int " + headerName + " = 1;";
						}
					});
					CompileJob job = compiler.createCompileJob(type);
					job.setSource(input);
					textArea_1.setText(job.compile());
				}
			}
		});
		mnFile.add(mntmLoad);
	}
}
