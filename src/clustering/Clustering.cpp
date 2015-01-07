#include "Clustering.h"
#include "Util.h"
#include <cstddef>

Clustering::Clustering(std::string seqDB, std::string seqDBIndex,
        std::string alnDB, std::string alnDBIndex,
        std::string outDB, std::string outDBIndex,
        int validateClustering, int maxListLen){

    Debug(Debug::WARNING) << "Init...\n";
    Debug(Debug::INFO) << "Opening sequence database...\n";
    seqDbr = new DBReader(seqDB.c_str(), seqDBIndex.c_str());
    seqDbr->open(DBReader::SORT);

    Debug(Debug::INFO) << "Opening alignment database...\n";
    alnDbr = new DBReader(alnDB.c_str(), alnDBIndex.c_str());
    alnDbr->open(DBReader::NOSORT);

    dbw = new DBWriter(outDB.c_str(), outDBIndex.c_str());
    dbw->open();

    this->validate = validateClustering;
    this->maxListLen = maxListLen;
    Debug(Debug::INFO) << "done.\n";
}

void Clustering::run(int mode){

    struct timeval start, end;
    gettimeofday(&start, NULL);

    std::list<set *> ret;
    Clustering::set_data set_data;
    if (mode == Parameters::SET_COVER){
        Debug(Debug::INFO) << "Clustering mode: SET COVER\n";
        Debug(Debug::INFO) << "Reading the data...\n";
        set_data = read_in_set_data();

        Debug(Debug::INFO) << "\nInit set cover...\n";
        SetCover setcover(set_data.set_count,
                          set_data.uniqu_element_count,
                          set_data.max_weight,
                          set_data.all_element_count,
                          set_data.element_size_lookup
                         );

        Debug(Debug::INFO) << "Adding sets...\n";
        for(size_t i = 0; i < set_data.set_count; i++){
            setcover.add_set(i + 1, set_data.set_sizes[i], //TODO not sure why +1
                    (const unsigned int*)   set_data.sets[i],
                    (const unsigned short*) set_data.weights[i],
                    set_data.set_sizes[i]);
        }

        Debug(Debug::WARNING) << "Calculating the clustering.\n";
        ret = setcover.execute_set_cover();
        Debug(Debug::INFO) << "done.\n";

        Debug(Debug::INFO) << "Writing results...\n";
        writeData(ret);
        Debug(Debug::INFO) << "...done.\n";
    }
    else if (mode == Parameters::GREEDY){
        Debug(Debug::INFO) << "Clustering mode: GREEDY\n";
        Debug(Debug::INFO) << "Reading the data...\n";
        set_data = read_in_set_data();

        Debug(Debug::INFO) << "Init simple clustering...\n";
        SimpleClustering simpleClustering(set_data.set_count,
                set_data.uniqu_element_count,
                set_data.all_element_count,
                set_data.element_size_lookup);

        for(size_t i = 0; i < set_data.set_count; i++){
            simpleClustering.add_set((const unsigned int*)set_data.sets[i],
                    set_data.set_sizes[i]);
        }

        Debug(Debug::WARNING) << "Calculating the clustering...\n";
        ret = simpleClustering.execute();
        std::cout << "ret size: " << ret.size() << "\n";

        Debug(Debug::INFO) << "done.\n";

        Debug(Debug::INFO) << "Writing results...\n";
        writeData(ret);
        Debug(Debug::INFO) << "...done.\n";
    }
    else{
        Debug(Debug::ERROR)  << "ERROR: Wrong clustering mode!\n";
        exit(EXIT_FAILURE);
    }

    if (validate == 1){
        Debug(Debug::INFO) << "Validating results...\n";
        if(validate_result(&ret,set_data.uniqu_element_count))
            Debug(Debug::INFO) << " VALID\n";
        else
            Debug(Debug::INFO) << " NOT VALID\n";
    }

    unsigned int dbSize = alnDbr->getSize();
    unsigned int seqDbSize = seqDbr->getSize();
    unsigned int cluNum = ret.size();

    seqDbr->close();
    alnDbr->close();
    dbw->close();
    delete seqDbr;
    delete alnDbr;
    delete dbw;

    gettimeofday(&end, NULL);
    int sec = end.tv_sec - start.tv_sec;
    Debug(Debug::INFO) << "\nTime for clustering: " << (sec / 60) << " m " << (sec % 60) << "s\n\n";

    Debug(Debug::INFO) << "\nSize of the sequence database: " << seqDbSize << "\n";
    Debug(Debug::INFO) << "Size of the alignment database: " << dbSize << "\n";
    Debug(Debug::INFO) << "Number of clusters: " << cluNum << "\n";

    delete[] set_data.startWeightsArray;
    delete[] set_data.startElementsArray;
    delete[] set_data.weights;
    delete[] set_data.sets;
    delete[] set_data.set_sizes;
    delete[] set_data.element_size_lookup;

}

void Clustering::writeData(std::list<set *> ret){

    size_t BUFFER_SIZE = 1000000;
    char* outBuffer = new char[BUFFER_SIZE];
    std::list<set *>::const_iterator iterator;
    for (iterator = ret.begin(); iterator != ret.end(); ++iterator) {
        std::stringstream res;
        set::element * element =(*iterator)->elements;
        // first entry is the representative sequence
        char* dbKey = seqDbr->getDbKey(element->element_id);
        do{
            char* nextDbKey = seqDbr->getDbKey(element->element_id);
            res << nextDbKey << "\n";
        }while((element=element->next)!=NULL);

        std::string cluResultsOutString = res.str();
        const char* cluResultsOutData = cluResultsOutString.c_str();
        if (BUFFER_SIZE < strlen(cluResultsOutData)){
            Debug(Debug::ERROR) << "Tried to process the clustering list for the query " << dbKey
                                << " , length of the list = " << ret.size() << "\n";
            Debug(Debug::ERROR) << "Output buffer size < clustering result size! (" << BUFFER_SIZE << " < " << cluResultsOutString.length()
                                << ")\nIncrease buffer size or reconsider your parameters -> output buffer is already huge ;-)\n";
            continue;
        }
        memcpy(outBuffer, cluResultsOutData, cluResultsOutString.length()*sizeof(char));
        dbw->write(outBuffer, cluResultsOutString.length(), dbKey);
    }
    delete[] outBuffer;
}


bool Clustering::validate_result(std::list<set *> * ret,unsigned int uniqu_element_count){
    std::list<set *>::const_iterator iterator;
    unsigned int result_element_count=0;
    int* control = new int [uniqu_element_count+1];
    memset(control, 0, sizeof(int)*(uniqu_element_count+1));
    for (iterator = ret->begin(); iterator != ret->end(); ++iterator) {
        set::element * element =(*iterator)->elements;
        do{ 
            control[element->element_id]++;
            result_element_count++;
        }while((element=element->next)!=NULL);
    }
    int notin = 0;
    int toomuch = 0;
    for (unsigned int i = 0; i < uniqu_element_count; i++){
        if (control[i] == 0){
            Debug(Debug::INFO) << "id " << i << " (key " << seqDbr->getDbKey(i) << ") is missing in the clustering!\n";
            Debug(Debug::INFO) << "len = " <<  seqDbr->getSeqLens()[i] << "\n";
            Debug(Debug::INFO) << "alignment results len = " << strlen(alnDbr->getDataByDBKey(seqDbr->getDbKey(i))) << "\n";
            notin++;
        }
        else if (control[i] > 1){
            Debug(Debug::INFO) << "id " << i << " (key " << seqDbr->getDbKey(i) << ") is " << control[i] << " times in the clustering!\n";
            toomuch = toomuch + control[i];
        }
    }
    if (notin > 0)
        Debug(Debug::INFO) << "not in the clustering: " << notin << "\n";
    if (toomuch > 0)
        Debug(Debug::INFO) << "multiple times in the clustering: " << toomuch << "\n";
    delete[] control;
    if (uniqu_element_count==result_element_count)
        return true;
    else{
        Debug(Debug::ERROR) << "uniqu_element_count: " << uniqu_element_count
                            << ", result_element_count: " << result_element_count << "\n";
        return false;
    }
}


Clustering::set_data Clustering::read_in_set_data(){

    Clustering::set_data ret_struct;

    // n = overall sequence count
    unsigned int n = seqDbr->getSize();
    // m = number of sets
    unsigned int m = alnDbr->getSize();

    ret_struct.uniqu_element_count = n;
    ret_struct.set_count = m;

    // set element size lookup
    unsigned int * element_buffer = new unsigned int[n];
    unsigned int * element_size   = new unsigned int[n + 2];
    memset(element_size, 0, sizeof(unsigned int) * (n + 2));
    ret_struct.element_size_lookup = element_size;
    // return set size
    unsigned int * set_size=new unsigned int[m];
    ret_struct.set_sizes = set_size;
    // set sets
    unsigned int ** sets = new unsigned int*[m];
    ret_struct.sets = sets;
    // set weights
    unsigned short ** weights = new unsigned short*[m];
    ret_struct.weights = weights;
    ret_struct.max_weight = 0;
    ret_struct.all_element_count = 0;

    int empty = 0;
    // needed for parsing
    const unsigned int ELEMENTS_IN_RECORD = 2;
    char * words[ELEMENTS_IN_RECORD];
    memset(words, 0, ELEMENTS_IN_RECORD*sizeof(char*));
    char * dbKey = new char[255+1];

    // count lines to know the target memory size
    const char * data = alnDbr->getData();
    size_t dataSize = alnDbr->getDataSize();
    size_t elementCount = Util::count_lines(data, dataSize);
    unsigned int * elements = new unsigned int[elementCount];
    unsigned short * weight = new unsigned short[elementCount];
    std::fill_n(weight, elementCount, 1);
    size_t curr_start_pos = 0;

        // the reference id of the elements is always their id in the sequence database
    for(unsigned int i = 0; i < m; i++) {
        Log::printProgress(i);
        //TODO fox problem with no entry
        char* data = alnDbr->getData(i);
        unsigned int element_counter = 0;

        unsigned int elementSize = Util::getWordsOfLine(data, words, ELEMENTS_IN_RECORD);
        if(elementSize != 2){ // check if file contains entry
            Debug(Debug::ERROR) << "ERROR: Sequence " << i
            << " does not containe any sequence!\n";
            continue;
        }
        ptrdiff_t keySize =  (words[1] - words[0]) - 1;
        strncpy(dbKey, data, keySize);
        dbKey[keySize] = '\0';
        size_t cnt = 0;
        while (*data != '\0' && cnt < this->maxListLen)
        {
            size_t curr_element = seqDbr->getId(dbKey);
            if (curr_element == UINT_MAX || curr_element > seqDbr->getSize()){
                Debug(Debug::ERROR) << "ERROR: Element " << dbKey
                        << " contained in some alignment list, but not contained in the sequence database!\n";
                EXIT(EXIT_FAILURE);
            }
            // add an edge
            element_buffer[element_counter++] = (unsigned int) curr_element;
            element_size[curr_element]++;
            ret_struct.all_element_count++;
            // next db key
            data = Util::skipLine(data);
            Util::getWordsOfLine(data, words, ELEMENTS_IN_RECORD);
            keySize = (words[1] - words[0]) - 1;
            strncpy(dbKey, data, keySize);
            dbKey[keySize] = '\0';
            cnt++;
        }

        if (cnt == 0){
            empty++;
        }
        // max_weight can not be gibber than 2^16
        if(element_counter > SHRT_MAX){
            Debug(Debug::ERROR)  << "ERROR: Set has too much elements. Set name is "
                      << dbKey << " and has has the weight " << element_counter <<".\n";
        }
        ret_struct.max_weight = std::max((unsigned short)element_counter, ret_struct.max_weight);
        // set pointer
        memcpy(elements + curr_start_pos, element_buffer, sizeof(unsigned int) * element_counter);
        weights[i] = (weight + curr_start_pos);
        sets[i] = (elements + curr_start_pos);
        set_size[i] = element_counter;
        curr_start_pos += element_counter;
    }
    ret_struct.startElementsArray = elements;
    ret_struct.startWeightsArray = weight;
    if (empty > 0)
        Debug(Debug::WARNING) << empty << " input sets were empty!\n";
    delete [] element_buffer;
    delete [] dbKey;
    return ret_struct;
}

