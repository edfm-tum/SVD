#include "model.h"
#include "tools.h"
#include "strtools.h"

#include <QThreadPool>

Model *Model::mInstance = 0;

Model::Model(const std::string &fileName)
{
    if (mInstance!=nullptr)
        throw std::logic_error("Creation of model: model instance ptr is not 0.");
    mInstance = this;
    mYear = -1; // not set up

    if (!Tools::fileExists(fileName))
        throw std::logic_error("Error: The configuration file '" + fileName + "' does not exist.");
    mSettings.loadFromFile(fileName);
    auto split_path = splitPath(fileName);
    Tools::setProjectDir( split_path.first );

    // set up logging
    inititeLogging();

    lg_setup->info("Model Setup, config file: '{}', project root folder: '{}'", fileName, split_path.first);

}

Model::~Model()
{
    shutdownLogging();
    mInstance = nullptr;
}

bool Model::setup()
{
    // general setup
    bool mt = settings().valueBool("model.multithreading",true);
    if (mt)
        QThreadPool::globalInstance()->setMaxThreadCount( QThread::idealThreadCount() );
    else
        QThreadPool::globalInstance()->setMaxThreadCount( 1 );

    // set up model components
    setupSpecies();
    mStates = std::shared_ptr<States>(new States());
    mStates->setup();

    mClimate = std::shared_ptr<Climate>(new Climate());
    mClimate->setup();

    mLandscape = std::shared_ptr<Landscape>(new Landscape());
    mLandscape->setup();

    mYear = 0; // model is set up, ready to run
    return true;

}

void Model::finalizeYear()
{
    // increment residence time for all pixels (updated pixels go from 0 -> 1)
    for (Cell &c : landscape()->futureGrid()) {
        if (!c.isNull())
            c.setResidenceTime( c.residenceTime() + 1);
    }


    landscape()->switchStates();
    stats.NPackagesTotalSent += stats.NPackagesSent;
    stats.NPackagesTotalDNN += stats.NPackagesDNN;
}

void Model::newYear()
{
    if (mYear == 0) {
        // first year of the simulation: copy the current to the modifyable landscape:
        landscape()->futureGrid().copy(landscape()->currentGrid());
    }

    stats.NPackagesSent = stats.NPackagesDNN = 0;
    // increment the counter
    mYear = mYear + 1;
    // other initialization ....
}

void Model::inititeLogging()
{
    settings().requiredKeys("logging", {"file", "setup.level", "model.level", "dnn.level"});

    std::string log_file = Tools::path(settings().valueString("logging.file"));


    // asynchronous logging, 2 seconds auto-flush
    spdlog::set_async_mode(8192, spdlog::async_overflow_policy::block_retry,
                           nullptr,
                           std::chrono::seconds(2));

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::simple_file_sink_mt>(log_file));

    //sinks.push_back(std::make_shared<my_threaded_sink>(ui->lLog));
    std::vector<std::string> levels = SPDLOG_LEVEL_NAMES; // trace, debug, info, ...

    auto combined_logger = spdlog::create("main", sinks.begin(), sinks.end());
    combined_logger->flush_on(spdlog::level::err);

    int idx = indexOf(levels, settings().valueString("logging.model.level"));
    if (idx==-1)
        throw std::logic_error("Setup logging: the value '" + settings().valueString("logging.model.level") + "' is not a valid logging level. Valid are: " + join(levels) );
    combined_logger->set_level(spdlog::level::level_enum(idx) );

    combined_logger=spdlog::create("setup", sinks.begin(), sinks.end());
    combined_logger->flush_on(spdlog::level::err);
    idx = indexOf(levels, settings().valueString("logging.setup.level"));
    if (idx==-1)
        throw std::logic_error("Setup logging: the value '" + settings().valueString("logging.setup.level") + "' is not a valid logging level. Valid are: " + join(levels) );
    combined_logger->set_level(spdlog::level::level_enum(idx) );


    combined_logger=spdlog::create("dnn", sinks.begin(), sinks.end());
    combined_logger->flush_on(spdlog::level::err);
    idx = indexOf(levels, settings().valueString("logging.dnn.level"));
    if (idx==-1)
        throw std::logic_error("Setup logging: the value '" + settings().valueString("logging.dnn.level") + "' is not a valid logging level. Valid are: " + join(levels) );
    combined_logger->set_level(spdlog::level::level_enum(idx) );


    //auto combined_logger = std::make_shared<spdlog::logger>("console", begin(sinks), end(sinks));

    //register it if you need to access it globally
    //spdlog::register_logger(combined_logger);
    lg_main = spdlog::get("main");
    lg_setup = spdlog::get("setup");

    lg_main->info("Started logging. Log levels: main: {}, setup: {}, dnn: {}", levels[lg_main->level()], levels[lg_setup->level()], levels[spdlog::get("dnn")->level()]);

}

void Model::shutdownLogging()
{
    if (lg_main)
        lg_main->info("Shutdown logging");

    spdlog::apply_all([&](std::shared_ptr<spdlog::logger> l)
    {
        l->flush();
    });
    spdlog::drop_all();


}

void Model::setupSpecies()
{
    std::string species_list = settings().valueString("model.species");
    mSpeciesList = split(species_list, ',');
    for (std::string &s : mSpeciesList)
        s = trimmed(s);

    lg_setup->debug("Setup of species: N={}.", mSpeciesList.size());

    if (lg_setup->should_log(spdlog::level::trace)) {
        lg_setup->trace("************");
        lg_setup->trace("Species: {}", join(mSpeciesList));
        lg_setup->trace("************");
    }

}