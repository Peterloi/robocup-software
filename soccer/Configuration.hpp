#pragma once

#include <QFile>
#include <QStringList>
#include <vector>
#include <QDomDocument>

class QTreeWidget;
class QTreeWidgetItem;
class Configuration;
class ConfigItem;
/**
 * static variable for the program, extremely general
 */
class Configuration: public QObject
{
	Q_OBJECT;
	
	public:
		Configuration();
		
		void tree(QTreeWidget *tree);
		
		QTreeWidget *tree() const
		{
			return _tree;
		}

		// name lookup - returns pointer if exists, null otherwise
		ConfigItem *nameLookup(const QString& name) const;

		bool load(const QString &filename, QString &error);
		bool save(const QString &filename, QString &error);

	protected Q_SLOTS:
		void itemChanged(QTreeWidgetItem *item, int column);
		
	protected:
		friend class ConfigItem;
		
		// Called by ConfigItem's constructor
		void addItem(ConfigItem *item);
		
		void addToTree(ConfigItem *item);
		
		QList<ConfigItem *> _allItems;
		
		QTreeWidget *_tree;
		
		ConfigItem *configItem(QTreeWidgetItem *ti);
		
		QDomDocument _doc;
};

/**
 * Base class for items in configuration: this is constructed through
 * functions in the Configuration class which ensure new items are added
 * to the tree properly
 */
class ConfigItem
{
protected:
		ConfigItem(Configuration *tree, const QString &name);

public:
		virtual ~ConfigItem();
		
		// A ConfigItem's name is a sequence of path segments separated by '/'.
		// The path is a list of these segments in order.
		const QStringList &path() const
		{
			return _path;
		}
		
		virtual QString toString() = 0;
		
		// Called by Configuration when the user changes the value
		virtual void setValue(const QString &str) = 0;
		
	protected:
		friend class Configuration;
		
		void addItem()
		{
			_config->addItem(this);
		}
		
		// Called when the tree item is first created
		virtual void setupItem();
		
		// Called by subclasses to update item text
		void valueChanged(const QString &str);
		
		QStringList _path;
		Configuration *_config;
		QTreeWidgetItem *_treeItem;
};

class ConfigBool: public ConfigItem
{
	public:
	ConfigBool(Configuration *tree, QString name, bool value = false);

		bool value();
		
		operator bool()
		{
			return value();
		}
		
		bool operator=(bool x)
		{
			_value = x;
			setupItem();
			return x;
		}
		
		virtual QString toString();
		virtual void setValue(const QString &str);
	
	protected:
		friend class Configuration;
		virtual void setupItem();
		
		bool _value;
};

class ConfigInt: public ConfigItem
{
	public:
	ConfigInt(Configuration *tree, QString name, int value = 0);

		virtual QString toString();
		virtual void setValue(const QString &str);
		
		operator int() const
		{
			return _value;
		}
		
		int operator=(int x)
		{
			value(x);
			return x;
		}
		
		int value() const
		{
			return _value;
		}
		
		void value(int v)
		{
			_value = v;
			valueChanged(QString::number(v));
		}
	
	protected:
		friend class Configuration;
		int _value;
};

class ConfigDouble: public ConfigItem
{
	public:
		ConfigDouble(Configuration *tree, QString name, double value = 0);
		
		virtual QString toString();
		virtual void setValue(const QString &str);
		
		operator double() const
		{
			return _value;
		}
		
		double operator=(double x)
		{
			value(x);
			return x;
		}
		
		double value() const
		{
			return _value;
		}
		
		void value(double v)
		{
			_value = v;
			valueChanged(QString::number(v));
		}
	
	protected:
		friend class Configuration;
		double _value;
};

#define REGISTER_CONFIGURABLE(x) static ConfigurableImpl<x> x##__configurable;
/**
 *
 */
class Configurable
{
public:
	Configurable();
	virtual ~Configurable() {}
	
	virtual void createConfiguration(Configuration *cfg) const = 0;
	
	static const std::list<Configurable *> &configurables();
	
private:
	static std::list<Configurable *> *_configurables;
};
/**
 * a template for making configurables
 * the implementing object is responsible for handling configuration
 */
template<class T>
class ConfigurableImpl: public Configurable
{
public:
	virtual void createConfiguration(Configuration *cfg) const
	{
		T::createConfiguration(cfg);
	}
};
