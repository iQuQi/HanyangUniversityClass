import {Component} from 'react';
import TimerForm from './Form'
import Timer from './Timer';
import TimerUp from './TimerUp'
import TimerControl from './TimerControl';
import TimerTitle from './TimerTitle';
import TimerHistory from './TimerHistory';

import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import { CardActionArea } from '@mui/material';
import Axios from "axios";

import {API,Auth} from 'aws-amplify';
import {listTasks} from '../graphql/queries';
import {taskByCreatedAt} from '../graphql/queries';
import {createTask} from '../graphql/mutations';
import {withAuthenticator} from '@aws-amplify/ui-react';
let time = 1500;
class TimerPannel extends Component{
	constructor(){
		super();
		

		this.state = {
		initialized : false,
		remained: time,
		activated: false,
		paused: false,
		
		errorTextField:false,
		helperTextField: "",

		timerValue : time,
		timerTitle: "",
		currentTitle: "",
		currentTask: {},

		helperText: "",

		history: []
		};
	}

	
	componentDidMount(){
/*
		Axios.post('http://localhost:4000/graphql',
		{ 'query': '{tasks {id title duration}}','variables': null},
		{'Content-Type' : 'application/json'})
		.then(res => this.setState({history: res.data.data.tasks}))
		.catch(e=>console.log(e));
*/
		Auth.currentAuthenticatedUser()
		.then(res=>{
			this.setState({user:{username: res.username, ...res.attributes}});
			})
		.catch(e=>console.log(e));

		API.graphql({query: taskByCreatedAt , variables: {groupId :'001'}})
		.then(res=> this.setState({history:res.data.taskByCreatedAt.items}))
		.catch(e=> console.log(e));
	}

	handleStartTimer = () => {
		let {timerValue} = this.state;
		this.setState({
			remained: timerValue
		})
		this.interval= setInterval(
		()=>{
			this.setState((prev) => {
				if(prev.remained <= 0){
					clearInterval(this.interval);
					return{activated: false,initialized : false};
				}
				else{
				return { 
					initialized : true,
					activated: true,
					remained: prev.remained-1
				};
				}
			});//???????????? setState()
		},1000);//???????????? setInterval()
		//1???(1000)?????? ??? ??????????????? ?????????????????? ???
	}

	handleResumeTimer = () => {
		this.interval= setInterval(
		()=>{
			this.setState((prev) => {
				if(prev.remained <= 0){
					clearInterval(this.interval);
					return{activated: false, initialized : false};
				}else{
				return { 
					//?????? ?????? ????????? ????????????
					paused: false,
					remained: prev.remained-1
				};
				}
			});
		},1000);
 	}

	handleStopTimer = () => {
		let {timerValue} = this.state;
		//Interval ???????????? ??????
		clearInterval(this.interval);
		this.setState(()=>{
			//?????? ????????? ????????? ??????
			return	{
			currentTask: {},
			initialized: false,
			activated: false,
			remained  : timerValue
			};
		});
	}

	handlePauseTimer = () => {
		clearInterval(this.interval);
		this.setState(()=>{
			//?????? ?????? ????????? ?????????
			return{ 
			paused:true};
		});
	}

	handleChangeSlider = e => {
		if (Number(e.target.value) < 5){
			this.setState({
			helperText: "error",
			timerValue: 5 * 60})
		}
		else{
			this.setState({
				helperText:"",
				timerValue: Number(e.target.value)*60
			})
		}
	}

	handleChangeTextField = e => {
		if(e.target.value && e.target.value.length>25){
			this.setState({errorTextField: true,
			helperTextField: "Title cannat exceeds 25 char",
			timerTitle: e.target.value})	;
		}else{
			this.setState({errorTextField: false,
			helperTextField: "" ,
			timerTitle: e.target.value});
		}
	}

	handleSubmit = e =>{
		e.preventDefault();
		console.log(e.target);
		// e.target??? ????????? ??? ????????? ?????????
		//[...e.target] -> ????????????????????? spread ???????????? ????????? array??? ???????????????
		let { timerTitle,timerValue,history} =this.state;

		if(timerTitle === ""){
			this.setState(
			{
				errorTextField: true,
				helperTextField: "Empty Title"
			}
			);
		}else{
		//??????????????? ?????????????????? ??? ???????????? ????????? ???
		/*let newTask = {id:history.length, 
			title: timerTitle, 
			duration: timerValue};
		*/

		/*
			Axios.post('http://localhost:4000/graphql',
			{ 'query': 
			`mutation{ addTask( title: "${timerTitle}", duration : ${timerValue})
			{id title duration}}`,
			'variables': null},
			{'Content-Type' : 'application/json'})
			.then(res =>{ 
			let newTask = res.data.data.addTask;
			this.setState({
				errorTextField: false,
				helperTextField: "",
				currentTitle: timerTitle,
				currentTask: newTask,
				history: [...history, newTask],
				timerTitle: ""

			});
			this.handleStartTimer();
			}).catch(e=> console.log(e));
		*/
			API.graphql({query: createTask, variables: {input: {groupId: '001', title: timerTitle, duration: timerValue}}})
			.then(res=>{
				let newTask = res.data.createTask;
				this.setState({
					errorTextField: false,
					helperTextField: "",
					currentTitle: timerTitle,
					currentTask: newTask,
					history: [...history, newTask] ,
					timerTitle: ""
				})
				this.handleStartTimer();
				})
			.catch(e=>console.log(e));

		}


	}

	render() {
		let {currentTask,history,errorTextField,currentTitle ,helperTextField,timerTitle,timerValue,initialized,remained, activated, paused,helperText} =this.state;
		return (<Card sx={{maxWidth : 360}}>
			<CardActionArea>
			<CardMedia component="img" height = {140}
			//?????? ?????? ????????? ?????? ?????? ????????? ???????????? ???
			image="https://mui.com/static/images/cards/contemplative-reptile.jpg"
          	alt="green iguana" />
			<CardContent>
			<TimerHistory 
			activated={activated}
			history={history} 
			currentTask={currentTask}
			paused= {paused}
			initialized={initialized}
			/>
			<TimerTitle activated ={activated} taskTitle = {currentTitle}/>
			<Timer remained={remained} />
			<TimerForm
				errorTextField = {errorTextField}
				helperTextField= {helperTextField}
				timerTitle={timerTitle}
				timerValue = {timerValue}
				helperText = {helperText}
				activated ={activated}
				handleChangeSlider={this.handleChangeSlider}
				handleChangeTextField ={this.handleChangeTextField}
				handleSubmit={this.handleSubmit}
				/>
			<TimerControl
				activated = {activated}
				paused = {paused}
				handleStopTimer = {this.handleStopTimer}
				handleStartTimer = {this.handleSubmit}
				handlePauseTimer = {this.handlePauseTimer}
				handleResumeTimer = {this.handleResumeTimer}
				/>
			<TimerUp 
				handleClose = {this.handleStopTimer}
				open = {remained <= 0}
			/>
			</CardContent>
			</CardActionArea>
			</Card>);
	}
}

export default withAuthenticator(TimerPannel);
