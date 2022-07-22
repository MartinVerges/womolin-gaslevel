
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

  let selectedScale = undefined;
  async function endStep1 () { step = 2; }
  async function endStep2 () { 
    fetch(`/api/setup/empty?scale=${selectedScale}`, {
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
    fetch(`/api/setup/weight?scale=${selectedScale}`, {
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
</script>

<svelte:head>
  <title>Sensor Calibration</title>
</svelte:head>

<div class="container">
    <h4>Calibration - Step {step} of 3 {#if selectedScale > 0} of scale {selectedScale}{/if}</h4>
{#if step == 1 || step == undefined}
    <p>
        We have to calibrate the weight in order to use it propperly.
        This usually only needs to be done once, insofar as you are using identical bottles.
        However, small deviations in the 1% range may occur due to slight differences in the bottle weights.
    </p>
    <p>
        Which scale do you want to calibrate? <br>
        <select bind:value={selectedScale} id="scale" class="form-select">
            <option value="{undefined}" disabled selected="selected">please select a scale</option>
        {#each {length: numScales} as _, i}
            <option value="{(i+1)}">{(i+1)}. Scale</option>
        {/each}
        </select>
        {#if selectedScale > 0}
        <Button on:click={endStep1} block>Start calibration</Button>            
        {/if}
    </p>
{:else if step == 2}
    <p>
        Please <b>remove all weights</b> from the scale. Make sure that the scale is stable and correctly positioned in the water.
    </p>
    <p>
        In this step, the sensor is calibrated to 0 and then a known weight is measured in the next step.
    </p>
    <Button on:click={endStep2} block>The scale is empty</Button>
    {:else}
    <p>
        You should now place a relatively heavy object, whose exact weight you know, on the scale.
        If necessary, you can measure something in advance with another scale.
    </p>
    <Label for="weight">Weight of the item in grams</Label>
    <Input id="weight" type="number" min="0" bind:value={data.weight}/>
    {#if data.weight > 0}
    <p>The weight is on the scale and the correct weight has been entered?</p>
    <Button on:click={endStep3} block>Complete calibration</Button>
    {/if}
{/if}
</div>

{#if sensorValue}
<div class="row">
  <div class="col-sm-1">ID</div>
  <div class="col-sm-1">Level</div>
  <div class="col-sm-1">sensorValue</div>  
  <div class="col-sm-1">airPressure</div>  
  <div class="col-sm-1">temperature</div>  
{#each sensorValue as sensor}
  <div class="col-sm-1">{sensor.id}</div>
  <div class="col-sm-1">{sensor.level}</div>
  <div class="col-sm-1">{sensor.sensorValue}</div>  
  <div class="col-sm-1">{sensor.airPressure}</div>  
  <div class="col-sm-1">{sensor.temperature}</div>  
{/each}
</div>
{/if}
