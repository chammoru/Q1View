#include "RawOpenDialog.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "qimage_cs.h"

bool RawOpenDialog::getOptions(QWidget *parent, RawOpenOptions *options)
{
	if (!options) {
		return false;
	}

	QDialog dialog(parent);
	dialog.setWindowTitle(QObject::tr("Open raw image"));

	QLineEdit *fileEdit = new QLineEdit(options->fileName, &dialog);
	QPushButton *browseButton = new QPushButton(QObject::tr("Browse..."), &dialog);

	QHBoxLayout *fileLayout = new QHBoxLayout;
	fileLayout->addWidget(fileEdit);
	fileLayout->addWidget(browseButton);

	QSpinBox *widthSpin = new QSpinBox(&dialog);
	widthSpin->setRange(1, 32768);
	widthSpin->setValue(options->width);

	QSpinBox *heightSpin = new QSpinBox(&dialog);
	heightSpin->setRange(1, 32768);
	heightSpin->setValue(options->height);

	QComboBox *formatCombo = new QComboBox(&dialog);
	for (size_t i = 0; i < ARRAY_SIZE(qcsc_info_table); ++i) {
		if (qcsc_info_table[i].cs_load_info && qcsc_info_table[i].csc2rgb888) {
			formatCombo->addItem(QString::fromLatin1(qcsc_info_table[i].name));
		}
	}

	const int currentIndex = formatCombo->findText(options->colorSpaceName);
	if (currentIndex >= 0) {
		formatCombo->setCurrentIndex(currentIndex);
	}

	QFormLayout *form = new QFormLayout;
	form->addRow(QObject::tr("File"), fileLayout);
	form->addRow(QObject::tr("Width"), widthSpin);
	form->addRow(QObject::tr("Height"), heightSpin);
	form->addRow(QObject::tr("Format"), formatCombo);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

	QVBoxLayout *root = new QVBoxLayout(&dialog);
	root->addLayout(form);
	root->addWidget(buttons);

	QObject::connect(browseButton, &QPushButton::clicked, &dialog, [&dialog, fileEdit]() {
		const QString fileName = QFileDialog::getOpenFileName(&dialog, QObject::tr("Open raw image"));
		if (!fileName.isEmpty()) {
			fileEdit->setText(fileName);
		}
	});

	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, fileEdit]() {
		if (fileEdit->text().isEmpty()) {
			QMessageBox::warning(&dialog, QObject::tr("Open raw image"), QObject::tr("Choose a raw image file."));
			return;
		}

		dialog.accept();
	});
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}

	options->fileName = fileEdit->text();
	options->width = widthSpin->value();
	options->height = heightSpin->value();
	options->colorSpaceName = formatCombo->currentText();
	return true;
}
