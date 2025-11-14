
#include "modbus_business.h"
#include "protocol_process.h"

ModbusBusiness::ModbusBusiness()
{

}

ModbusBusiness::~ModbusBusiness()
{
   stop();
}

int ModbusBusiness::start()
{
   int ret = 0;
   if(enable_contorl_){
   	printf("not contorl x, z\n");
	return ret;
   }
   //std::thread keyboard_thread(&ModbusBusiness::keyboard_listener,this);
   //keyboard_thread.detach();
   modbusProtocol_ptr_ = std::make_shared<ModbusProtocol>();
   modbusProtocol_ptr_->start();
   sleep(2);
#if 0
   std::thread thread_(&ModbusBusiness::get_length,this);
   thread_.detach();
   printf("ModbusBusiness::start()\n");
   parse_thread_ = std::thread(&ModbusBusiness::processQueue, this);
   parse_thread_.detach();
#else
    movement_thread_ = std::thread(&ModbusBusiness::processMovement, this);
    polling_thread_ = std::thread(&ModbusBusiness::pollPositions, this);
    movement_thread_.detach();
    polling_thread_.detach();
    printf("ModbusBusiness started successfully.\n");
#endif
   command_map_ = {
            {"a1", &ModbusProtocol::OneAxisSubdivisionCommand},
            {"b1", &ModbusProtocol::TwoAxisSubdivisionCommand},
            {"a2", &ModbusProtocol::OneAxisPitchCommand},
            {"b2", &ModbusProtocol::TwoAxisPitchCommand},
            {"a3", &ModbusProtocol::OneAxisReductionRatioCommand},
            {"b3", &ModbusProtocol::TwoAxisReductionRatioCommand},
            {"a4", &ModbusProtocol::OneAxisSpeedTimeCommand},
            {"b4", &ModbusProtocol::TwoAxisSpeedTimeCommand},
            {"a5", &ModbusProtocol::OneAxisDecelerateTimeCommand},
            {"b5", &ModbusProtocol::TwoAxisDecelerateTimeCommand},
            {"a6", &ModbusProtocol::OneAxisLengthCommand},
            {"b6", &ModbusProtocol::TwoAxisLengthCommand},
            {"a7", &ModbusProtocol::OneAxisRunnSpeedCommand},
            {"b7", &ModbusProtocol::TwoAxisRunnSpeedCommand},
            {"a8", &ModbusProtocol::OneAxisReturnOriginSpeedCommand},
            {"b8", &ModbusProtocol::TwoAxisReturnOriginSpeedCommand},
            {"a9", &ModbusProtocol::OneAxisBackOrigin},
            {"b9", &ModbusProtocol::TwoAxisBackOrigin},
            {"a10", &ModbusProtocol::OneAxisCoordinatesClear},
            {"b10", &ModbusProtocol::TwoAxisCoordinatesClear},
            //{"a11", &ModbusProtocol::OneAxisCurrentCoordinates},
            //{"b11", &ModbusProtocol::TwoAxisCurrentCoordinates},
            {"a12", &ModbusProtocol::SystemEmergencyStop}
        };
   return ret;
}

void ModbusBusiness::stop()
{
     if(enable_contorl_) return;
     if(!run_){	
	run_ = true;
	cv_.notify_one();
     }
}
void ModbusBusiness::resetTask(int task_id)
{    
	std::lock_guard<std::mutex> lock(mutex_data_);    
	if (current_task_id_ > 0 && current_task_id_ == task_id) 
	{       printf("INFO: Resetting Modbus controller task %d due to external cancellation.\n", task_id);        
		// Reset all task-related state variables        
		current_task_id_ = -1;        
		// Clear any pending commands for the cancelled task        
		std::queue<QueuedTaskEx> empty_queue;        
		data_queue_ex_.swap(empty_queue);        
		// Reset movement state flags        
		x_need_run_ = false;        
		z_need_run_ = false;        
		is_req = false;       
		cut_ = 0;
		active_axis_ = '\0';
		// Reset the callback to prevent it from being called erroneously later        
		if (current_callback_) {            current_callback_ = nullptr;        }    
	}
}

void ModbusBusiness::axisRunLength(const std::string &axis, const std::string &length)
{
   printf("run axis = %s length = %s\n",axis.c_str(),length.c_str());
   if(axis == "X"){
	if(x_length_ != length){
   	    modbusProtocol_ptr_->OneAxisLengthCommand2(length);
	    x_length_ = length;
	    x_need_run_ = true;
	    printf("one length = %s\n",length.c_str());
	}else{
	    printf("set X_length No changes  = %s\n",x_length_.c_str());
	    //if(current_callback_) { current_callback_(true); return; }
	}
	cut_ = 0;
	x_need_run_ = true;
	is_req = true;
   }else if(axis == "Z"){
	if(z_length_ != length){
   	    modbusProtocol_ptr_->TwoAxisLengthCommand2(length);
	    z_length_ = length;
	    z_need_run_ = true;
	    printf("two length = %s\n",length.c_str());
	}else{	
	    printf("set Z_length No changes  = %s\n",x_length_.c_str());
	    //if(current_callback_) { current_callback_(true); return; }
	}
	cut_ = 0;
	z_need_run_ = true;
	is_req = true;
   }else{
   	printf("axis = %s is error ...\n",axis.c_str());
   }
}

void ModbusBusiness::processQueue()
{
   while(true){
       if(stop_flag_){
	  break;
       }
       if(data_queue_.empty()){
          if (current_task_id_ != -1 && !x_need_run_ && !z_need_run_) {
              // Queue is empty and no movements are pending, so the task is done.
              current_task_id_ = -1;
          }
	  std::this_thread::sleep_for(std::chrono::milliseconds(300));
	  continue;
       }
       if(x_need_run_ || z_need_run_){
       	  std::this_thread::sleep_for(std::chrono::milliseconds(300));
	  continue;
       }

       QueuedTask task;
       {
	   std::unique_lock<std::mutex> lock(mutex_data_);
	   task = data_queue_.front();
	   data_queue_.pop();
       }

        // Store the callback before initiating the move
        current_callback_ = task.cb;
	axisRunLength(task.para.axis, task.para.value);
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
   }
}

void ModbusBusiness::pushData(const Para &data, MovementCallback cb, int task_id)
{
   std::lock_guard<std::mutex> lock(mutex_data_);
   printf(">>>>>>>>>>Push zhuantai kongzhi axis = %s value = %s\n",data.axis.c_str(),data.value.c_str());
   if (current_task_id_ != -1 && current_task_id_ != task_id) {
       printf("ERROR: Modbus controller is busy with task %d. Rejecting new task %d.\n", current_task_id_, task_id);
       if (cb) cb(false, "push data error"); // Report failure if busy with a DIFFERENT task
       return;
   }

   if (current_task_id_ == -1) {
       current_task_id_ = task_id;
       printf("INFO: Modbus controller starting new task %d.\n", task_id);
   }

   data_queue_.push({data, cb});
}

void ModbusBusiness::pushCompositeTask(const CompositeMovementTask& task, MovementCallback cb, int task_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    printf("2222222\n");
    if (current_task_id_ != -1) {
	printf("---------000-----\n");
        if (cb) cb(false, "error task id");
        return;
    }
    if(enable_contorl_ || (task.x_value == "-80000" && task.z_value == "-80000")){
   	if(cb) cb(true, "g,f move seccuse");
	printf("3333333\n");
       	return;
    }
    printf(".............\n");
    data_queue_ex_.push({task_id, task, cb});
    cv_.notify_one();
}

void ModbusBusiness::processMovement() {
    while (!run_) {
        QueuedTaskEx current_task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return !data_queue_ex_.empty() || run_; });
            if (run_) break;
	    if(data_queue_ex_.empty()) continue;
            current_task = data_queue_ex_.front();
            data_queue_ex_.pop();
            current_task_id_ = current_task.task_id;
            verification_pending_ = false;
        }
        bool x_success = run_axis_to_completion(current_task.task_id, "X", current_task.task.x_value);
        bool z_success = false;
        if (x_success) {
            z_success = run_axis_to_completion(current_task.task_id, "Z", current_task.task.z_value);
        }
	printf("x_success = %d z_success = %d \n",x_success, z_success);
        bool overall_success = x_success && z_success;
	std::string msg = "g,f move failed";
        if ((current_task_id_ != -1) && current_task.cb) {
	    if(overall_success) msg = "g,f move seccuse";
            current_task.cb(overall_success, msg);
        }
	else {
            ProtocolProcess protocol_process_;
            protocol_process_.sendResults("ok", 2);
        }
	if(overall_success){
            verification_countdown_ = 30;
            verification_pending_ = true;
	}
	resetTask(current_task_id_);
        current_task_id_ = -1;
    }
}

bool ModbusBusiness::run_axis_to_completion(int task_id, const std::string& axis, const std::string& target_value_str) {
    std::string* last_commanded_val_ptr = (axis == "X") ? &last_x_commanded_ : &last_z_commanded_;
    
    {
	if(axis == "X") printf("XXXXXX Run to = %s\n",target_value_str.c_str());
	else if(axis == "Z") printf("ZZZZZ Run to = %s\n",target_value_str.c_str());
        //printf("1111111111111111111111\n");
        std::lock_guard<std::mutex> lock(last_command_mutex_);
        if (target_value_str == "-80000" || target_value_str == *last_commanded_val_ptr) {
            return true;
        }
    }
    if (current_task_id_ != task_id) return false;
    uint16_t target_pos = std::stoi(target_value_str);
    active_axis_ = axis[0]; // Set the active axis for the polling thread
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (axis == "X") {
        modbusProtocol_ptr_->OneAxisLengthCommand2(target_value_str);
    } else {
        modbusProtocol_ptr_->TwoAxisLengthCommand2(target_value_str);
    }
    
    {
        std::lock_guard<std::mutex> lock(last_command_mutex_);
        *last_commanded_val_ptr = target_value_str;
    }

    // Wait for 2 seconds (4 cycles of 500ms)
    //printf("2222222222222222222222222\n");
    for(int i = 0; i < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if(run_ || current_task_id_ != task_id) {
	    printf("current id = %d task id = %d\n",current_task_id_, task_id);
            active_axis_ = '\0';
            return false;
        }
    }
    int timeout_wait = 100;
    //printf("3333333333333333333333\n");
    while (!run_) {
        if (current_task_id_ != task_id) { 
            active_axis_ = '\0';
            return false; 
        }
        uint16_t current_pos = (axis == "X") ? current_x_pos_.load() : current_z_pos_.load();
        if (current_pos == target_pos) {
            printf("INFO: Axis %s reached target %u.\n", axis.c_str(), target_pos);
            active_axis_ = '\0'; // Clear the active axis
            return true;
        }
	if(timeout_wait-- < 0){
	    active_axis_ = '\0';
	    return false;
	}
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    active_axis_ = '\0';
    //printf("44444444444444444\n");
    printf(" %s run over .................\n",axis.c_str());
    return true;
}

void ModbusBusiness::pollPositions() {
    const int poll_interval_ms = 500*2;
    const int idle_poll_cycles = 4/2; // 4 * 500ms = 2 seconds
    int idle_counter = 0;

    while(!run_) {
        char currently_active = active_axis_.load();
        if (currently_active == 'X') {
            current_x_pos_ = modbusProtocol_ptr_->OneAxisCurrentCoordinates();
            idle_counter = 0;
        } else if (currently_active == 'Z') {
            current_z_pos_ = modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
            idle_counter = 0;
        } else { // Idle
            if (++idle_counter >= idle_poll_cycles) {
                current_x_pos_ = modbusProtocol_ptr_->OneAxisCurrentCoordinates();
                current_z_pos_ = modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
                printf("IDLE POLL: X=%u, Z=%u\n", current_x_pos_.load(), current_z_pos_.load());
                idle_counter = 0;
            }
            if (verification_pending_) {
                if (verification_countdown_-- <= 0) {
                    //printf("VERIFY: 5-second verification period over. Checking final positions.\n");
                    //uint16_t final_x = modbusProtocol_ptr_->OneAxisCurrentCoordinates();
                    //uint16_t final_z = modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
                    uint16_t final_x = current_x_pos_;
                    uint16_t final_z = current_z_pos_;
                    if (std::to_string(final_x) != last_x_commanded_) {
                        printf("VERIFY: X-axis drifted. Updating last known position from %s to %u.\n", last_x_commanded_.c_str(), final_x);
                        last_x_commanded_ = std::to_string(final_x);
                    }
                    if (std::to_string(final_z) != last_z_commanded_) {
                        printf("VERIFY: Z-axis drifted. Updating last known position from %s to %u.\n", last_z_commanded_.c_str(), final_z);
                        last_z_commanded_ = std::to_string(final_z);
                    }
		    verification_countdown_ = 30;
                    //verification_pending_ = false;
                }
            }
	}
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    }
}

void ModbusBusiness::axisRunSpeed(const std::string &axis, const std::string &speed)
{
   if(enable_contorl_) return;	
   printf("run axis = %s speed = %s\n",axis.c_str(),speed.c_str());
   if(axis == "X"){
	if(x_speed_ != speed){
   	   modbusProtocol_ptr_->OneAxisRunnSpeedCommand2(speed);
	   x_speed_ = speed;
	}
   }else if(axis == "Z"){
	if(z_speed_ != speed){
   	   modbusProtocol_ptr_->TwoAxisRunnSpeedCommand2(speed);
	   z_speed_ = speed;
	}
   }else{
   	printf("axis = %s is error ...\n",axis.c_str());
   }
}

void ModbusBusiness::get_length()
{
  uint8_t print_cut = 0;
   while(!run_){
	if(nullptr != modbusProtocol_ptr_){
#if 1
		if(x_need_run_){
		   std::this_thread::sleep_for(std::chrono::milliseconds(800));
		   current_x_length_ = modbusProtocol_ptr_->OneAxisCurrentCoordinates();
		}else if(z_need_run_){
		   std::this_thread::sleep_for(std::chrono::milliseconds(800));
		   current_z_length_ = modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
		}else {
		   std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		   current_x_length_ = modbusProtocol_ptr_->OneAxisCurrentCoordinates();
		   //std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		   current_z_length_ = modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
		}
		if(print_cut++ % 2 == 0 || true) printf("zhuantai ======x = %d =====z = %d======= x_need_run_ = %d z_need_run_ = %d\n",current_x_length_,current_z_length_,x_need_run_,z_need_run_);
		//if(x_length_ == "") x_length_ = std::to_string(current_x_length_);
		//if(z_length_ == "") z_length_ = std::to_string(current_z_length_);
		bjlenght();
		//modbusProtocol_ptr_->readData();
#endif
	}
   	//std::this_thread::sleep_for(std::chrono::milliseconds(2000));
   }
}


bool ModbusBusiness::bjlenght()
{
    if(is_req){
    	//is_req = false;
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	if(cut_++ < 3) { return false; }
	uint16_t v_x = std::atoi(x_length_.c_str());
	uint16_t v_z = std::atoi(z_length_.c_str());
	//printf("vx = %d x length = %d vz = %d z length = %d\n",v_x, current_x_length_, v_z, current_z_length_);
	printf("x_need_run_ = %d z_need_run_ = %d\n",x_need_run_, z_need_run_);
	if(x_need_run_){
	   if(v_x == current_x_length_) {
		x_need_run_ = false; cut_ = 0;
		printf("X AXIS Run to destination\n");
           	if (current_callback_) {
               	    current_callback_(true, "X Axis Run ok"); // Execute the WebSocket callback
           	}
	   }
	}else if(z_need_run_){
	   if(v_z == current_z_length_) {
		z_need_run_ = false; cut_ = 0;
		printf("Z AXIS Run to destination\n");
           	if (current_callback_) {
               	    current_callback_(true, "Z Axis Run ok"); // Execute the WebSocket callback
           	}
	   }
	}
	else if(!x_need_run_ && !z_need_run_ && v_x == current_x_length_ && v_z == current_z_length_){
	   is_req = false;
           cut_ = 0;
           if (current_callback_) {
               //current_callback_(true); // Execute the WebSocket callback
           } else {
               ProtocolProcess protocol_process_;
               protocol_process_.sendResults("ok", 2); // Fallback to HTTP feedback
           }
           //current_callback_ = nullptr; // Reset after use
	   if(current_task_id_ >= 0) resetTask(current_task_id_);
	   printf("...........zhuan tai.......dao da ........................\n");
	   return true;
	}
    }
    return false;
}


void ModbusBusiness::keyboard_listener()
{
    std::string input;
    std::regex pattern("(a(1[0-9]|20|[1-9])|b(1[0-9]|20|[1-9]))");
    while (!run_) {
        std::getline(std::cin, input); // 读取用户输入
        std::smatch match;
        if (std::regex_match(input, match, pattern)) {
            std::cout << "Received valid input: " << input << std::endl;
	    if(input == "a1"){
	    	std::cout << "a1 run " << std::endl;
		modbusProtocol_ptr_->OneAxisSubdivisionCommand();
	    }else if(input == "b1"){
	    	modbusProtocol_ptr_->TwoAxisSubdivisionCommand();
	    }else if(input == "a2"){
	    	modbusProtocol_ptr_->OneAxisPitchCommand();
	    }else if(input == "b2"){
	    	modbusProtocol_ptr_->TwoAxisPitchCommand();
	    }else if(input == "a3"){
	    	modbusProtocol_ptr_->OneAxisReductionRatioCommand();
	    }else if(input == "b3"){
	    	modbusProtocol_ptr_->TwoAxisReductionRatioCommand();
	    }else if(input == "a4"){
	    	modbusProtocol_ptr_->OneAxisSpeedTimeCommand();
	    }else if(input == "b4"){
	    	modbusProtocol_ptr_->TwoAxisSpeedTimeCommand();
	    }else if(input == "a5"){
	    	modbusProtocol_ptr_->OneAxisDecelerateTimeCommand();
	    }else if(input == "b5"){
	    	modbusProtocol_ptr_->TwoAxisDecelerateTimeCommand();
	    }else if(input == "a6"){
	    	modbusProtocol_ptr_->OneAxisLengthCommand();
	    }else if(input == "b6"){
	    	modbusProtocol_ptr_->TwoAxisLengthCommand();
	    }else if(input == "a7"){
	    	modbusProtocol_ptr_->OneAxisRunnSpeedCommand();
	    }else if(input == "b7"){
	    	modbusProtocol_ptr_->TwoAxisRunnSpeedCommand();
	    }else if(input == "a8"){
	    	modbusProtocol_ptr_->OneAxisReturnOriginSpeedCommand();
	    }else if(input == "b8"){
	    	modbusProtocol_ptr_->TwoAxisReturnOriginSpeedCommand();
	    }else if(input == "a9"){
	    	modbusProtocol_ptr_->OneAxisBackOrigin();
	    }else if(input == "b9"){
	    	modbusProtocol_ptr_->TwoAxisBackOrigin();
	    }else if(input == "a10"){
	    	modbusProtocol_ptr_->OneAxisCoordinatesClear();
	    }else if(input == "b10"){
	    	modbusProtocol_ptr_->TwoAxisCoordinatesClear();
	    }else if(input == "a11"){
	    	//modbusProtocol_ptr_->OneAxisCurrentCoordinates();
	    }else if(input == "b11"){
	    	//modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
	    }else if(input == "a12"){
	    	modbusProtocol_ptr_->SystemEmergencyStop();
	    }
        }else {	
            std::cout << "Received other data input: " << input << std::endl;
	}
    }
}

void ModbusBusiness::execute_command(const std::string& input) {
        auto it = command_map_.find(input);
        if (it != command_map_.end()) {
            auto command = it->second;
            std::cout << "Received valid input: " << input << std::endl;
	    ((*modbusProtocol_ptr_).*command)();
        } else {
            std::cout << "Received invalid input: " << input << std::endl;
        }
}


void ModbusBusiness::execute_command(const std::string& input, const std::string value)
{
    if(input == "aa1"){
        modbusProtocol_ptr_->OneAxisSubdivisionCommand2(value);
    }else if(input == "bb1"){
        modbusProtocol_ptr_->TwoAxisSubdivisionCommand2(value);
    }else if(input == "aa2"){
        modbusProtocol_ptr_->OneAxisPitchCommand2(value);
    }else if(input == "bb2"){
        modbusProtocol_ptr_->TwoAxisPitchCommand2(value);
    }else if(input == "aa3"){
        modbusProtocol_ptr_->OneAxisReductionRatioCommand2(value);
    }else if(input == "bb3"){
        modbusProtocol_ptr_->TwoAxisReductionRatioCommand2(value);
    }else if(input == "aa4"){
        modbusProtocol_ptr_->OneAxisSpeedTimeCommand2(value);
    }else if(input == "bb4"){
        modbusProtocol_ptr_->TwoAxisSpeedTimeCommand2(value);
    }else if(input == "aa5"){
        modbusProtocol_ptr_->OneAxisDecelerateTimeCommand2(value);
    }else if(input == "bb5"){
        modbusProtocol_ptr_->TwoAxisDecelerateTimeCommand2(value);
    }else if(input == "aa6"){
        modbusProtocol_ptr_->OneAxisLengthCommand2(value);
    }else if(input == "bb6"){
        modbusProtocol_ptr_->TwoAxisLengthCommand2(value);
    }else if(input == "aa7"){
        modbusProtocol_ptr_->OneAxisRunnSpeedCommand2(value);
    }else if(input == "bb7"){
        modbusProtocol_ptr_->TwoAxisRunnSpeedCommand2(value);
    }else if(input == "aa8"){
        modbusProtocol_ptr_->OneAxisReturnOriginSpeedCommand();
    }else if(input == "bb8"){
        modbusProtocol_ptr_->TwoAxisReturnOriginSpeedCommand();
    }else if(input == "aa9"){
        modbusProtocol_ptr_->OneAxisBackOrigin();
    }else if(input == "bb9"){
        modbusProtocol_ptr_->TwoAxisBackOrigin();
    }else if(input == "aa10"){
        modbusProtocol_ptr_->OneAxisCoordinatesClear();
    }else if(input == "bb10"){
        modbusProtocol_ptr_->TwoAxisCoordinatesClear();
    }else if(input == "aa11"){
        //modbusProtocol_ptr_->OneAxisCurrentCoordinates();
    }else if(input == "bb11"){
        //modbusProtocol_ptr_->TwoAxisCurrentCoordinates();
    }else if(input == "aa12"){
        modbusProtocol_ptr_->SystemEmergencyStop();
    }
}


