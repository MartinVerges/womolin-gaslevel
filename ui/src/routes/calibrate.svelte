
<script>
  import { variables } from '$lib/utils/variables';
  import { onMount } from 'svelte';
  import {
    Label,
    Input,
    Button
  } from 'sveltestrap';
  import { toast } from '@zerodevx/svelte-toast';

  // ******* SHOW STATUS ******** //
  let sensorValue = undefined;
  onMount(async () => { 
    if (!!window.EventSource) {
      var source = new EventSource('/api/events');

      source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected");
        }
      }, false);

      source.addEventListener('status', function(e) {
        try {
          sensorValue = JSON.parse(e.data);
        } catch (error) {
          console.log(error);
          console.log("Error parsing status", e.data);          
        }
      }, false);
    }
  })

  let step = 1;
  let numScales = 0;
  let selectedScale = undefined;

  let bottle = {
    emptyWeightGramms: 0,
    fullWeightGramms: 0,
  }
  async function getBottleWeights() {
    bottle.emptyWeightGramms = 0;
    bottle.fullWeightGramms = 0;
    const response = await fetch(`/api/calibrate/bottleweight?scale=${selectedScale}`, {
      headers: { "Content-type": "application/json" }
    }).catch(error => console.log(error))
    if(response.ok) {
      let data = await response.json();
      if ('emptyWeightGramms' in data) bottle.emptyWeightGramms = data.emptyWeightGramms;
      if ('fullWeightGramms' in data) bottle.fullWeightGramms = data.fullWeightGramms;
    } else {
      toast.push(`Error ${response.status} ${response.statusText}<br>Unable to receive current bottle weights.`, variables.toast.error)
    }
	};

  async function setNewBottleWeight() {
    fetch(`/api/calibrate/bottleweight?scale=${selectedScale}`, {
      method: 'POST',
      body: JSON.stringify(bottle),
      headers: { "Content-type": "application/json" }
    }).then(response => {
      if (response.ok) {
        step = 1;
        selectedScale = undefined;
        toast.push(`New bottle weight set`, variables.toast.success)
      } else {
        toast.push(`Error ${response.status} ${response.statusText}<br>Unable to write the empty value into the configuration.`, variables.toast.error)
      }
    }).catch(error => console.log(error))
  }

  onMount(async () => {
    const response = await fetch(`/api/level/num`, {
      headers: { "Content-type": "application/json" }
    }).catch(error => console.log(error))
    if(response.ok) {
      let data = await response.json();
      numScales = data.num || 0;
    } else {
      toast.push(`Error ${response.status} ${response.statusText}<br>Unable to receive current settings.`, variables.toast.error)
    }
	});

  async function endStep1 () { step = 2; }
  async function endStep2 () {
    fetch(`/api/calibrate/empty?scale=${selectedScale}`, {
      method: 'POST',
      body: '{}',
      headers: { "Content-type": "application/json" }
    }).then(response => {
      if (response.ok) {
        step = 3;
      } else {
        toast.push(`Error ${response.status} ${response.statusText}<br>Unable to write the empty value into the configuration.`, variables.toast.error)
      }
    }).catch(error => console.log(error))
  }
  let data = { weight: 0 };
  async function endStep3 () {
    fetch(`/api/calibrate/weight?scale=${selectedScale}`, {
      method: 'POST',
      body: JSON.stringify(data),
      headers: { "Content-type": "application/json" }
    }).then(response => {
      if (response.ok) {
        step = 1;
        selectedScale = undefined;
        toast.push(`Scale successfully calibrated`, variables.toast.success)
      } else {
        toast.push(`Error ${response.status} ${response.statusText}<br>Unable to write the empty value into the configuration.`, variables.toast.error)
      }
    }).catch(error => console.log(error))
  }

  async function abortSetupStep() {
    step = 1;
    selectedScale = undefined;
  }
</script>

<svelte:head>
  <title>Sensor Calibration</title>
</svelte:head>

<h4>Calibration - Step {step} of 3 {#if selectedScale > 0} of scale {selectedScale}{/if}</h4>
{#if step == 1 || step == undefined}
<p>
  We have to calibrate the weight in order to use it propperly.
  This usually only needs to be done once, insofar as you are using identical bottles.
  However, small deviations in the 1% range may occur due to slight differences in the bottle weights.
</p>
<p>
  Which scale do you want to calibrate? <br>
  <select bind:value={selectedScale} on:change={getBottleWeights} id="scale" class="form-select">
      <option value="{undefined}" disabled selected="selected">please select a scale</option>
  {#each {length: numScales} as _, i}
      <option value="{(i+1)}">{(i+1)}. Scale</option>
  {/each}
  </select>
</p>
{#if selectedScale > 0}
<p>
  If the weight of the gas bottle has changed, you can enter it here.
  The value printed on the bottle serves as a reference value, but we recommend using your own scale for this.
</p>
<div class="row">
  <div class="col-sm-4">
    <Label for="emptyWeight">Weight of the <b>empty</b> bottle in grams</Label>
    <Input id="emptyWeight" type="number" min="0" bind:value={bottle.emptyWeightGramms}/>
  </div>
  <div class="col-sm-4">
    <Label for="fullWeight">Weight of the <b>full</b> bottle in grams</Label>
    <Input id="fullWeight" type="number" min="0" bind:value={bottle.fullWeightGramms}/>
  </div>
  <div class="col-sm-4 align-self-end">
    <Button on:click={setNewBottleWeight} block>Set new bottle weight</Button>
  </div>
</div>

{/if}
{#if selectedScale > 0}
<br>
<p>
  <Button on:click={endStep1} class="btn-danger" block>Start initial calibration</Button>            
</p>
{/if}

{:else if step == 2}
<p>
    Please <b>remove all weights</b> from the scale. Make sure that the scale is stable and correctly positioned in the water.
</p>
<p>
    In this step, the sensor is calibrated to 0 and then a known weight is measured in the next step.
</p>
<div class="row">
  <div class="col-sm-6">
    <Button on:click={endStep2} class="btn-success" block>The scale is empty</Button>
  </div>
  <div class="col-sm-6">
    <Button on:click={abortSetupStep} class="btn-danger" block>Abort setup</Button>
  </div>
</div>
{:else}
<p>
  You should now place a relatively heavy object, whose exact weight you know, on the scale.
  If necessary, you can measure something in advance with another scale.
</p>
<p>
  <Label for="weight">Weight of the item in grams</Label>
  <Input id="weight" type="number" min="0" bind:value={data.weight}/>
</p>
{#if data.weight > 0}
<p>
  The weight is on the scale and the correct weight has been entered?
</p>
<p>
  <Button on:click={endStep3} block>Complete calibration</Button>
</p>
{/if}
{/if}

{#if sensorValue}
<h5>Current Sensor data as debug information</h5>
<div class="row">
  <div class="col-sm-1">ID</div>
  <div class="col-sm-1">Level</div>
  <div class="col-sm-1">sensorValue</div>  
  <div class="col-sm-1">airPressure</div>  
  <div class="col-sm-1">temperature</div>
</div>
{#each sensorValue as sensor}
<div class="row">
  <div class="col-sm-1">{sensor.id}</div>
  <div class="col-sm-1">{sensor.level}</div>
  <div class="col-sm-1">{sensor.sensorValue}</div>  
  <div class="col-sm-1">{sensor.airPressure}</div>  
  <div class="col-sm-1">{sensor.temperature}</div>  
</div>
{/each}
{/if}
