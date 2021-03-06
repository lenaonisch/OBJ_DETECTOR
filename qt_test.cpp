#include "StdAfx.h"
#include "qt_test.h"

qt_test::qt_test(QWidget *parent)
	: QMainWindow(parent)
{
	have_forest = false;
//	single_image_selected = false;
	last_selected_result = -1;
	ui.setupUi(this);

	ui.lblInput->setBackgroundRole(QPalette::Base);
	ui.lblInput->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	ui.lblInput->setScaledContents(true);
	ui.btnAddPositive->setEnabled(false);
	ui.btnAddNegative->setEnabled(false);

	//ui.scrollAreaInput->setBackgroundRole(QPalette::Dark);
	ui.scrollAreaInput->setWidget(ui.lblInput);
	ui.treeResults->setColumnCount(3);
	QStringList strList;
	strList.append("Filename");
	strList.append("Total time");
	strList.append("Voting time");
	ui.treeResults->setHeaderLabels(strList);
	ui.treeResults->header()->show();

	ui.treeResults->addAction(ui.actionExpand_all);
}

void qt_test::on_actionExpand_all_triggered()
{
	ui.treeResults->expandAll();
}

qt_test::~qt_test()
{

}

void qt_test::on_actionLoad_config_file_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this,
         tr("Open config"), QDir::currentPath(), "All files (*.*);;Images (*.png *.xpm *.jpg)");

	QFile file(fileName);
	QFileInfo fi(file);
	QDir::setCurrent(fi.absolutePath());
	
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

	string filename = fileName.toLocal8Bit().constData();
	try
	{
		gall_forest_app.loadConfig(filename/*, mode*/);

		ShowMessage("Config file has been loaded successfully!", 1);	


		ui.actionBatch_detect->setEnabled(true);
		ui.actionTrain->setEnabled(true);
		ui.actionLoad_test_images->setEnabled(true);
		ui.actionDetect->setEnabled(true);

		ui.btnAddPositive->setEnabled(true);
		ui.btnAddNegative->setEnabled(true);

		have_forest = false;

		ui.lstClasses->clear();
		for (int i = 0; i < gall_forest_app.classes.size(); i++)
			ui.lstClasses->addItem(QString("class %1: %2").arg(i).arg(gall_forest_app.classes[i].c_str()));
	}
	catch (exception& e)
	{
		ShowMessage(e.what(),2);
	}
}

void qt_test::AddPositiveRectToTree(QTreeWidgetItem* node, cv::Rect* rect, int class_)
{
	QString text;
	if (class_ >= num_of_classes)
		text = QString("negative: %1, %2->%3, %4").arg(rect->x).arg(rect->y).arg(rect->width).arg(rect->height);
	else
		text = QString("%1: %2, %3->%4, %5").arg(gall_forest_app.classes[class_].c_str()).arg(rect->x).arg(rect->y).arg(rect->width).arg(rect->height);
	ui.actionSave_rectangles->setEnabled(true);
	QTreeWidgetItem* item = new QTreeWidgetItem(QStringList(text));
	item->setToolTip(0, text);
	node->addChild(item);
}

void qt_test::AddPositiveFile(QString filename)
{
	string fileName_std = filename.toStdString();
	QTreeWidgetItem* next = new QTreeWidgetItem(QStringList(QString(gall_forest_app.getFilename(fileName_std).c_str())));
	next->setToolTip(0, filename);
	ui.treeResults->addTopLevelItem(next);
}

void qt_test::AddPositiveFile(string filename)
{
	QTreeWidgetItem* next = new QTreeWidgetItem(QStringList(QString(gall_forest_app.getFilename(filename).c_str())));
	next->setToolTip(0, QString(filename.c_str()));
	ui.treeResults->addTopLevelItem(next);
}

void qt_test::DisplayPositiveFiles()
{
	ui.treeResults->clear();
	QList<QTreeWidgetItem *> items;
	for (int i = 0; i < filepaths.size(); ++i)
	{
		Results* res = &positive[filepaths[i]];
		QStringList strList;
		strList.append(QString(gall_forest_app.getFilename(filepaths[i]).c_str()));
		if (res->time.size() > 0)
		{
			strList.append(QString("   %1").arg(res->time[0]));
			strList.append(QString("   %1").arg(res->time[2]));
		}
		QTreeWidgetItem* next = new QTreeWidgetItem(strList);
		next->setFirstColumnSpanned(true);
		next->setToolTip(0, QString(filepaths[i].c_str()));
		for (int j = 0;j < res->classes.size(); j++)
		{
			AddPositiveRectToTree(next, &res->rects[j], res->classes[j]);
		}
		items.append(next);
	}
	ui.treeResults->addTopLevelItems(items);
}

void qt_test::on_actionTrain_triggered()
{
	try
	{
		TrainParameters* param_form = new TrainParameters;
		param_form->ui->edtBinaryTests->setText(QString("%1").arg(gall_forest_app.binary_tests_iterations));
		if (param_form->exec() == QDialog::Accepted)
		{
			gall_forest_app.binary_tests_iterations = param_form->binary_tests_iterations();
			gall_forest_app.ntrees = param_form->trees();
			//gall_forest_app.subsamples_pos = param_form->trainPart();	
			//gall_forest_app.useAllMarkedPos = param_form->bUseAllMarkedInGUI();
			gall_forest_app.fluctparam = param_form->bFluct();
			
			string tr_path = gall_forest_app.treepath;
			std::size_t found = tr_path.find_last_of("/");
			QDir dir(QString((gall_forest_app.configpath+tr_path.substr(0, found)).c_str()));
			QStringList lst = dir.entryList(QStringList(QString((tr_path.substr(found+1)+"???.txt").c_str())));
			
			int off_tree = lst.count();
			gall_forest_app.run_train(off_tree);
			have_forest = true;
		}
	}
	catch (exception& e)
	{
		ShowMessage(e.what(), 2);
	}
}

void qt_test::on_actionShow_leaves_triggered()
{
	try
	{
		gall_forest_app.show();
	}
	catch (exception& e)
	{
		ShowMessage(e.what(),2);
	}
	//ui->plainTextEdit_Console
}

void qt_test::on_actionBatch_detect_triggered()
{
	try
	{
		last_selected_result = -1;
		if (filepaths.size() == 0)
		{
			ShowMessage("No files loaded!");
			return;
		}
		for (int i = 0; i < filepaths.size(); i++)
			positive[filepaths[i]].processed = false;// workaroung to initialize positive variable
		TestParameters* param_form = new TestParameters;
		if (param_form->exec() == QDialog::Accepted)
		{
			gall_forest_app.crDetect.testParam = param_form->testParameters();
			gall_forest_app.run_detect(have_forest, positive);
			DisplayPositiveFiles();
		}
	}
	catch (exception& e)
	{
		ShowMessage(e.what(), 2);
	}
}

void qt_test::on_actionBatch_detect_several_param_triggered()
{
	try
	{
		last_selected_result = -1;
		if (filepaths.size() == 0)
		{
			ShowMessage("No files loaded!");
			return;
		}
		for (int i = 0; i < filepaths.size(); i++)
			positive[filepaths[i]].processed = false;// workaroung to initialize positive variable
		
		TestParam param;
		param.bSaveHoughSpace = false;
		param.prob_threshold = 0;
		int out_scale[] = {50,50,70,70,90, 90, 90, 90, 100,100,100};
		int threshold[] = {70,85,85,95,110,120,135,145,140,150,160};
		int kernel[]	= {5, 5, 5, 5, 7,  7,  7,  7,  7,  7,  7};

		for (int i = 0; i<16; i++)
		{
			// parameters
			param.out_scale = out_scale[i];
			param.max_treshold = threshold[i];
			param.kernel = kernel[i];
			//detection
			gall_forest_app.crDetect.testParam = param;
			gall_forest_app.run_detect(have_forest, positive);
			// save results
			QString fileNamePos = QString("%1%2%3%4%5%6")
				.arg(QDir::currentPath())
				.arg("/results/")
				.arg(param.out_scale)
				.arg(param.max_treshold)
				.arg(param.kernel)
				.arg("positive.txt");
			QString fileNameNeg = QString("%1%2%3%4%5%6")
				.arg(QDir::currentPath())
				.arg("/results/")
				.arg(param.out_scale)
				.arg(param.max_treshold)
				.arg(param.kernel)
				.arg("negative.txt");

			string filenamePos = fileNamePos.toLocal8Bit().constData();
			string filenameNeg = fileNameNeg.toLocal8Bit().constData();
			SaveRects(filenamePos, filenameNeg);
			// clear results
			on_btnDropAllResults_clicked();
		}
	}
	catch (exception& e)
	{
		ShowMessage(e.what(), 2);
	}
}

void qt_test::on_actionLoad_rectangles_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this,
         tr("Load rectangles"), QDir::currentPath(), "All files (*.*);;Images (*.png *.xpm *.jpg)");

	QFile file(fileName);
	QFileInfo fi(file);
	QDir::setCurrent(fi.absolutePath());
	
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

	string filename = fileName.toLocal8Bit().constData();
	try
	{
		std::vector<string> vFilenames; 
		std::vector<cv::Rect> vBBox; 
		std::vector<unsigned int> vClassNums;
		std::vector<int> width_aver;
		vector<cv::Point> centers;
		string temp = gall_forest_app.trainposfiles;
		gall_forest_app.trainposfiles = filename;
		gall_forest_app.loadTrainPosFile(vFilenames, vBBox, centers, vClassNums, width_aver);
		filepaths.clear();
		positive.clear();
		for (int i = 0; i<vFilenames.size();i++)
		{
			string filepath = gall_forest_app.configpath + gall_forest_app.impath + "/" + vFilenames[i];
			if (std::find(filepaths.begin(), filepaths.end(), filepath) == filepaths.end())
				filepaths.push_back(filepath);
			positive[filepath].processed = true;
			positive[filepath].push_back(vClassNums[i], vBBox[i], GREEN);
			
		}
		
		DisplayPositiveFiles();
		gall_forest_app.trainposfiles = temp;
	}
	catch (exception& e)
	{
		ShowMessage(e.what(),2);
	}
}

void qt_test::on_actionDetect_triggered()
{
	/*try
	{*/
		last_selected_result = -1;
		int img_index; //res_index not used
		GetSelectedImageResult(img_index);
		if (img_index < 0)
		{
			ShowMessage("No image to process!");	
			return;
		}
		if (positive[filepaths[img_index]].processed)
		{
			ShowMessage("Image has been already processed!");
			return;
		}
		TestParameters* param_form = new TestParameters;
		if (param_form->exec() == QDialog::Accepted)
		{
			gall_forest_app.crDetect.testParam = param_form->testParameters();
			gall_forest_app.run_detect(have_forest, filepaths[img_index], positive[filepaths[img_index]]);
			DisplayPositiveFiles();
		}
	/*}
	catch (exception& e)
	{
		ShowMessage(e.what(),2);
	}*/
}

void qt_test::on_actionOpen_triggered()
{
	QStringList mimeTypeFilters;
    foreach (const QByteArray &mimeTypeName, QImageReader::supportedMimeTypes())
        mimeTypeFilters.append(mimeTypeName);
    mimeTypeFilters.sort();
//    const QStringList picturesLocations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    QFileDialog dialog(this, tr("Open File"),
                      /* picturesLocations.isEmpty() ?*/ QDir::currentPath() /*: picturesLocations.last()*/);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setMimeTypeFilters(mimeTypeFilters);
    dialog.selectMimeTypeFilter("image/jpeg");

	if (dialog.exec() == QDialog::Accepted && dialog.selectedFiles().length()>0)
	{
		if (loadFile(dialog.selectedFiles().first(), 0))
		{
			string str = dialog.selectedFiles().first().toStdString();
			filepaths.push_back(str);
			AddPositiveFile(str);
			positive[str].processed = false; //workaround

			if (last_selected_result != -1)
				ui.treeResults->topLevelItem(last_selected_result)->setSelected(false);
			last_selected_result = filepaths.size()-1;
			ui.treeResults->topLevelItem(last_selected_result)->setSelected(true);
			if  (!gall_forest_app.configpath.empty())
			{
				ui.actionDetect->setEnabled(true);
			}
		}	
	}

	//QString fileName = QFileDialog::getOpenFileName(this,
 //        tr("Open XML File 1"), QDir::currentPath(), "All files (*.*);;Images (*.png *.xpm *.jpg)");

	//QFile file(fileName);
 //   if (!file.open(QIODevice::ReadOnly)) {
 //       return;
 //   }

	//QStringList stringList;
 //   QTextStream textStream(&file);

 //   while (!textStream.atEnd())
 //       stringList << textStream.readLine();

 //   file.close();

	////ui.lstOut->addItems(stringList);

}

bool qt_test::loadFile(const QString &fileName, Results* res)
{
    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Cannot load %1.").arg(QDir::toNativeSeparators(fileName)));
        setWindowFilePath(QString());
		ui.lblInput->setPixmap(QPixmap());
        ui.lblInput->adjustSize();
        return false;
    }
	
	QPixmap pix = QPixmap::fromImage(image);
	if (res != NULL)
	{
		QPainter *paint = new QPainter(&pix);

		for (int i = 0; i < res->rects.size(); i++)
		{
			paint->setPen(res->colors[i]);
			if (res->classes[i] < num_of_classes)
				paint->drawText(res->rects[i].x-2, res->rects[i].y-2, QString(gall_forest_app.classes[res->classes[i]].c_str()));
			QRect rect(res->rects[i].x, res->rects[i].y, res->rects[i].width, res->rects[i].height);
			paint->drawRect(rect);
		}
	}

	ui.lblInput->setPixmap(pix);
	ui.lblInput->adjustSize();
	scaleFactor = 1.0;

    setWindowFilePath(fileName);
    return true;
}

void qt_test::DrawRect(cv::Rect rect, int class_, QColor color)
{
	QPixmap pixmap = *(ui.lblInput->pixmap());
	QPainter painter(&pixmap);              

	QPen pen(color);
	pen.setWidth(3);
	painter.setPen(pen);
	QRect qrect(rect.x, rect.y, rect.width, rect.height);
	painter.drawRect(qrect);
	if (class_ < num_of_classes)
		painter.drawText(rect.x-2, rect.y-2, QString(gall_forest_app.classes[class_].c_str()));

	ui.lblInput->setPixmap(pixmap);     
}

void qt_test::DrawRects(Results* res)
{
	QPixmap pixmap = *(ui.lblInput->pixmap());
	QPainter painter(&pixmap);              

	if (res != NULL)
	{
		for (int i = 0; i < res->rects.size(); i++)
		{
			QPen pen(res->colors[i]);
			pen.setWidth(3);
			painter.setPen(pen);
			QRect qrect(res->rects[i].x, res->rects[i].y, res->rects[i].width, res->rects[i].height);
			painter.drawRect(qrect);
			if (res->classes[i] < num_of_classes)
				painter.drawText(qrect.x()-2, qrect.y()-2, QString(gall_forest_app.classes[res->classes[i]].c_str()));
		}
		ui.lblInput->setPixmap(pixmap); 
	}
}

void qt_test::scaleImage(double factor)
{
	Q_ASSERT(ui.lblInput->pixmap());
    scaleFactor *= factor;
    ui.lblInput->resize(scaleFactor * ui.lblInput->pixmap()->size());

    adjustScrollBar(ui.scrollAreaInput->horizontalScrollBar(), factor);
    adjustScrollBar(ui.scrollAreaInput->verticalScrollBar(), factor);
}

void qt_test::adjustScrollBar(QScrollBar *scrollBar, double factor)
{
    scrollBar->setValue(int(factor * scrollBar->value()
                            + ((factor - 1) * scrollBar->pageStep()/2)));
}

void qt_test::on_actionZoom_in_2_triggered()
{
	scaleImage(1.25);
}
void qt_test::on_actionZoom_out_2_triggered()
{
	scaleImage(0.8);
}

void qt_test::on_actionTest_local_max_triggered()
{
	//int squire = ui.lineEditSquire->text().toInt();
	int threshold = ui.lineEditThreshold->text().toInt();
	//ui.plainTextEdit_Console->appendPlainText(str);
	Size size;
	size.height = 48;
	size.width = 100;
	CRForestDetector crdet;
	vector<MaxPoint> locations;
	cv::Mat tmp;
	cvtColor( currentImage, tmp, CV_RGB2GRAY );
	tmp.convertTo(tmp, CV_8UC1);
	crdet.localMaxima(tmp, size, locations, 0, threshold);

	/////////
	 
	QPixmap* pix_ = new QPixmap(filepaths[0].c_str());
	
	QPainter *paint = new QPainter(pix_);

	paint->setPen(RED);
	for (int i = 0; i < locations.size(); i++)
	{
		paint->drawEllipse(locations[i].point.x, locations[i].point.y, 3,3);
		//paint->drawPoint(locations[i].point.x, locations[i].point.y);
		
	}
	

	ui.lblInput->setPixmap(*pix_);
	ui.lblInput->adjustSize();
	/////////
}

void qt_test::on_btnAddPositive_clicked()
{ 
	MouseLabel *lbl = ui.lblInput;
	if (lbl->dx > 0 && lbl->dy > 0)
	{
		int img_index;
		GetSelectedImageResult(img_index);
		int class_num = ui.lstClasses->currentIndex();
		img_index = (img_index >= 0) ? img_index: filepaths.size()-1;
		string filename_ = filepaths[img_index];
		cv::Rect rect(lbl->x, lbl->y, lbl->dx, lbl->dy);
		positive[filename_].push_back(class_num, rect, GREEN);
		QTreeWidgetItem* treeItem = ui.treeResults->topLevelItem(img_index);
		AddPositiveRectToTree(treeItem, &rect, class_num);
		ui.lblInput->rubberBand->hide();
		DrawRect(rect, class_num, GREEN);
	}
	else
	{
		ShowMessage("Nothing selected!");	
	}
}

void qt_test::on_btnAddNegative_clicked()
{ 
	int img_index, res_index;
	GetSelectedImageResult(img_index,res_index);
	if (res_index < 0)
	{
		ShowMessage("Nothing selected!");
		return;
	}
	string filename_ = filepaths[img_index];
	cv::Rect rect = positive[filename_].makeNegative(res_index);
	DrawRect(rect, num_of_classes, RED);
	QString text = QString("negative: %1, %2->%3, %4").arg(rect.x).arg(rect.y).arg(rect.width).arg(rect.height);
	QTreeWidgetItem* item = ui.treeResults->topLevelItem(img_index)->child(res_index);
	item->setText(0, text);
	item->setToolTip(0, text);
}

void qt_test::on_actionMean_shift_triggered()
{}

void qt_test::GetSelectedImageResult(int& img_index)
{
	img_index = -1;
	if (!ui.treeResults->currentItem())
	{
		return;
	}
	QTreeWidgetItem * parent = ui.treeResults->currentItem()->parent();
	
	if (parent != 0)
	{
		img_index = ui.treeResults->indexOfTopLevelItem(parent);
	}
	else
	{
		QModelIndex model = ui.treeResults->currentIndex();
		img_index = model.row();
	}
}

void qt_test::GetSelectedImageResult(int& img_index, int& res_index)
{
	res_index = -1;
	img_index = -1;
	if (!ui.treeResults->currentItem())
	{
		return;
	}
	QTreeWidgetItem * parent = ui.treeResults->currentItem()->parent();
	
	if (parent != 0)
	{
		res_index = ui.treeResults->currentIndex().row(); // index of result
		img_index = ui.treeResults->indexOfTopLevelItem(parent);
	}
	else
	{
		QModelIndex model = ui.treeResults->currentIndex();
		img_index = model.row();
	}
}

void qt_test::GetSelectedImageResult(string& img_name, int& img_index, int& res_index)
{
	QTreeWidgetItem * parent = ui.treeResults->currentItem()->parent();
	res_index = -1;
	img_index;
	if (parent != 0)
	{
		res_index = ui.treeResults->currentIndex().row(); // index of result
		img_name = parent->text(0).toStdString();
		img_index = ui.treeResults->indexOfTopLevelItem(parent);
	}
	else
	{
		img_name = ui.treeResults->currentItem()->text(0).toStdString();
		QModelIndex model = ui.treeResults->currentIndex();
		img_index = model.row();
	}
}

void qt_test::on_treeResults_clicked()
{
	int img_index, res_index;
	string text;
	GetSelectedImageResult(text, img_index, res_index);
	if (img_index != last_selected_result)
	{
		last_selected_result = img_index;
		Results* res = &positive[filepaths[img_index]];
		if (res->colors.size() == 0)
			for (int i = 0; i < res->classes.size(); i++)
				res->colors.push_back(GREEN);
		
		if (res_index >= 0)
		{
			res->colors[res_index] = VIOLET;
		}
		loadFile(QString((filepaths[img_index]).c_str()), res);
		if (res_index >= 0)
		{
			res->colors[res_index] = GREEN;
		}
	}
	else
	{
		if (res_index >= 0)
		{
			Results res = positive[filepaths[img_index]];
			res.markResWithColor(res_index);
			DrawRects(&res);
		}
		
	}

}

void qt_test::LoadRectsFromBWMasks()
{
	///////
	string outputfile = gall_forest_app.configpath+"/horses_rects.txt";
	ofstream out(outputfile);
	//////
	string dirwhite = gall_forest_app.configpath+"/white/";
	string list_file = dirwhite+"out.txt";
	const char * filename = (list_file).c_str();
	FILE * pFile = fopen (filename,"r");

	int N;
	fscanf(pFile, "%i", &N);
	out<<N<<endl;

	for (int i = 0; i<N; i++)
	{
		char buffer[200];
		fscanf(pFile, "%s", &buffer);
		string file = buffer;
		cv::Mat bw;
		bw = imread(dirwhite+file, 0);
		bw.convertTo(bw, CV_8UC1);

		int a,b,c,d;
		bool break_ = false;
		for (int y = 0; y<bw.rows;y++){
			for (int x = 0; x<bw.cols;x++){
				if (bw.at<uchar>(y,x) > 0){
					b = y;
					break_ = true;
					break;
				}
			}
			if (break_)
				break;
		}
		break_ = false;
		for (int x = 0; x<bw.cols;x++){
			for (int y = 0; y<bw.rows;y++){
				if (bw.at<uchar>(y,x) > 0){
					a = x;
					break_ = true;
					break;
				}
			}
			if (break_)
				break;
		}
		/////////////
		break_ = false;
		for (int y = bw.rows-1; y>0;y--){
			for (int x = 0; x<bw.cols;x++){
				if (bw.at<uchar>(y,x) > 0){
					d = y;
					break_ = true;
					break;
				}
			}
			if (break_)
				break;
		}

		break_ = false;
		for (int x = bw.cols-1; x>0;x--){
			for (int y = 0; y<bw.rows;y++){
				if (bw.at<uchar>(y,x) > 0){
					c = x;
					break_ = true;
					break;
				}
			}
			if (break_)
				break;
		}


		out<< file << '\t' << a << " " << b << " " << c << " " << d << " " << "0" << endl;
	}
	std::fclose(pFile);
	out.close();
}

void qt_test::on_actionLoad_test_images_triggered()
{
	try
	{
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, "Clean", "Remove previously downloaded files?",
			QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
		if (reply == QMessageBox::Yes) {
			filepaths.clear();
			gall_forest_app.loadImFile(filepaths);
		} else if (reply == QMessageBox::No){
			vector<string> text_files;
			gall_forest_app.loadImFile(text_files);
			filepaths.insert(filepaths.end(), text_files.begin(), text_files.end());
		}
		else return;
		DisplayPositiveFiles();

	}
	catch (exception& e)
	{
		ShowMessage(e.what(),2);
	}
}

void qt_test::on_btnRefresh_clicked()
{
	DisplayPositiveFiles();
}

void qt_test::on_btnDropAllResults_clicked()
{
	for (map<string, Results>::iterator it = positive.begin(); it != positive.end(); it++)
	{
		it->second.clear();
	}
	DisplayPositiveFiles();
	if (last_selected_result>=0 && filepaths.size() > last_selected_result)
		loadFile(filepaths[last_selected_result].c_str(), NULL);
	ui.actionSave_rectangles->setEnabled(false);
}

void qt_test::on_btnRemove_clicked()
{
	int img_index, res_index;
	GetSelectedImageResult(img_index, res_index);
	if (img_index < 0)
		return;
	if (res_index >= 0)
	{
		QTreeWidgetItem* rem = ui.treeResults->topLevelItem(img_index)->child(res_index);
		ui.treeResults->topLevelItem(img_index)->removeChild(rem);
		Results* res = &positive[filepaths[img_index]];
		res->removeAt(res_index);
		loadFile(QString((filepaths[img_index]).c_str()), res);
	}
	else
	{
		delete ui.treeResults->topLevelItem(img_index);
		positive.erase(positive.find(filepaths[img_index]));
		filepaths.erase(filepaths.begin()+img_index);
		if (img_index == filepaths.size())	
			img_index--;
		if (img_index < 0)
		{
			ui.lblInput->setPixmap(QPixmap());
			return;
		}
		ui.treeResults->topLevelItem(img_index)->setSelected(true);
		loadFile(QString((filepaths[img_index]).c_str()), &positive[filepaths[img_index]]);
	}
}

void qt_test::SaveRects(string filenamePos, string filenameNeg)
{
	try
	{
		ofstream outPos(filenamePos);
		ofstream outNeg(filenameNeg);
		if(outPos.is_open()&& outNeg.is_open()) {
			
			for (int i = 0; i < filepaths.size(); ++i)
			{
				Results* res = &positive[filepaths[i]];
				for (int r = 0; r < res->rects.count(); r++)
				{
					if (res->classes[r] != num_of_classes)
					{
						outPos<<gall_forest_app.getFilename(filepaths[i]).c_str() << " ";
						cv::Rect rect = res->rects[r];
						outPos<<rect.x <<" "<<rect.y<<" "<<rect.width<<" "<<rect.height<<" "<<res->classes[r]<<endl;
					}
					else
					{
						outNeg<<gall_forest_app.getFilename(filepaths[i]).c_str() << " ";
						cv::Rect rect = res->rects[r];
						outNeg<<rect.x <<" "<<rect.y<<" "<<rect.width<<" "<<rect.height<<endl;
					}
				}
			}
			outPos.close();
			outNeg.close();
		}
		else 
			throw string_exception("Can't write rects to files!");

	}
	catch (exception& e)
	{
		ShowMessage(e.what(),2);
	}
}

void qt_test::on_actionSave_rectangles_triggered()
{
	QString fileNamePos = QFileDialog::getSaveFileName(this,
         tr("Save detected and marked POSITIVE rectangles"), QDir::currentPath()+"/positive_detected.txt", "Text files (*.txt);");
	QString fileNameNeg = QFileDialog::getSaveFileName(this,
         tr("Save detected and marked NEGATIVE rectangles"), QDir::currentPath()+"/negative_detected.txt", "Text files (*.txt);");

	string filenamePos = fileNamePos.toLocal8Bit().constData();
	string filenameNeg = fileNameNeg.toLocal8Bit().constData();
	
	SaveRects(filenamePos, filenameNeg);
}

void qt_test::on_actionExport_detection_time_triggered()
{
	QString fileName = QFileDialog::getSaveFileName(this,
         tr("Export time spent on detection"), QDir::currentPath()+"/detection_time.csv", "CSV(*.csv); Text (*.txt); All files (*.*);");

	string filename = fileName.toLocal8Bit().constData();
	try
	{
		ofstream out(filename);
		if(out.is_open()) {
			out<<"Filename;Height;Width;Total time;Voting time;"<<endl;
			for (int i = 0; i < filepaths.size(); ++i)
			{
				out<<gall_forest_app.getFilename(filepaths[i]).c_str() << ";";
				Results* res = &positive[filepaths[i]];	
				out<<res->height<<";"<<res->width<<";"<<res->time[0] <<";"<<res->time[2]<<endl;
			}
			out.close();
		}
		else 
			throw string_exception("Can't write results to file!");

	}
	catch (exception& e)
	{
		ShowMessage(e.what(),2);
	}
}